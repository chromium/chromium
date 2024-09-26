// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/video_capture_manager.h"

#include <memory>
#include <set>
#include <utility>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/timer/elapsed_timer.h"
#include "build/android_buildflags.h"
#include "build/build_config.h"
#include "content/browser/media/media_internals.h"
#include "content/browser/renderer_host/media/video_capture_controller.h"
#include "content/browser/screenlock_monitor/screenlock_monitor.h"
#include "content/common/features.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/common/content_client.h"
#include "media/base/media_switches.h"
#include "media/base/video_facing.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "media/capture/video/video_capture_device.h"
#include "services/video_effects/public/mojom/video_effects_processor.mojom-forward.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"

namespace {

void LogVideoCaptureError(media::VideoCaptureError error) {
  base::UmaHistogramEnumeration("Media.VideoCapture.Error", error);
}

const base::UnguessableToken& FakeSessionId() {
  // TODO(crbug.com/40252973): Investigate whether there's a better way
  // to accomplish this (without using UnguessableToken::Deserialize).
  static const base::UnguessableToken fake_session_id(
      base::UnguessableToken::Deserialize(0xFFFFFFFFFFFFFFFFU,
                                          0xFFFFFFFFFFFFFFFFU)
          .value());
  return fake_session_id;
}

}  // namespace

namespace content {

// Class used for queuing request for starting a device.
class VideoCaptureManager::CaptureDeviceStartRequest {
 public:
  CaptureDeviceStartRequest(
      VideoCaptureController* controller,
      const media::VideoCaptureSessionId& session_id,
      const media::VideoCaptureParams& params,
      mojo::PendingRemote<video_effects::mojom::VideoEffectsProcessor>
          video_effects_processor);
  VideoCaptureController* controller() const { return controller_; }
  const base::UnguessableToken& session_id() const { return session_id_; }
  media::VideoCaptureParams params() const { return params_; }

  mojo::PendingRemote<video_effects::mojom::VideoEffectsProcessor>&&
  TakeVideoEffectsProcessor() {
    return std::move(video_effects_processor_);
  }

 private:
  const raw_ptr<VideoCaptureController> controller_;
  const base::UnguessableToken session_id_;
  const media::VideoCaptureParams params_;
  mojo::PendingRemote<video_effects::mojom::VideoEffectsProcessor>
      video_effects_processor_;
};

VideoCaptureManager::CaptureDeviceStartRequest::CaptureDeviceStartRequest(
    VideoCaptureController* controller,
    const media::VideoCaptureSessionId& session_id,
    const media::VideoCaptureParams& params,
    mojo::PendingRemote<video_effects::mojom::VideoEffectsProcessor>
        video_effects_processor)
    : controller_(controller),
      session_id_(session_id),
      params_(params),
      video_effects_processor_(std::move(video_effects_processor)) {}

VideoCaptureManager::VideoCaptureManager(
    std::unique_ptr<VideoCaptureProvider> video_capture_provider,
    base::RepeatingCallback<void(const std::string&)> emit_log_message_cb)
    : video_capture_provider_(std::move(video_capture_provider)),
      emit_log_message_cb_(std::move(emit_log_message_cb)) {
  ScreenlockMonitor* screenlock_monitor = ScreenlockMonitor::Get();
  if (screenlock_monitor) {
    screenlock_monitor->AddObserver(this);
  }
}

VideoCaptureManager::~VideoCaptureManager() {
  DCHECK(device_start_request_queue_.empty());
  ScreenlockMonitor* screenlock_monitor = ScreenlockMonitor::Get();
  if (screenlock_monitor) {
    screenlock_monitor->RemoveObserver(this);
  }
}

void VideoCaptureManager::AddVideoCaptureObserver(
    media::VideoCaptureObserver* observer) {
  DCHECK(observer);
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  capture_observers_.AddObserver(observer);
}

void VideoCaptureManager::RemoveAllVideoCaptureObservers() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  capture_observers_.Clear();
}

void VideoCaptureManager::RegisterListener(
    MediaStreamProviderListener* listener) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(listener);
  listeners_.AddObserver(listener);
#if BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_DESKTOP_ANDROID)
  application_state_has_running_activities_ = true;
  app_status_listener_ =
      base::android::ApplicationStatusListener::New(base::BindRepeating(
          &VideoCaptureManager::OnApplicationStateChange, this));
#endif
}

void VideoCaptureManager::UnregisterListener(
    MediaStreamProviderListener* listener) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  listeners_.RemoveObserver(listener);
}

void VideoCaptureManager::EnumerateDevices(
    EnumerationCallback client_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "VideoCaptureManager::EnumerateDevices");
  EmitLogMessage("VideoCaptureManager::EnumerateDevices", 1);

  // Pass a timer for UMA histogram collection.
  video_capture_provider_->GetDeviceInfosAsync(
      base::BindPostTaskToCurrentDefault(
          base::BindOnce(&VideoCaptureManager::OnDeviceInfosReceived, this,
                         base::ElapsedTimer(), std::move(client_callback))));
}

base::UnguessableToken VideoCaptureManager::Open(
    const blink::MediaStreamDevice& device) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "VideoCaptureManager::Open");

  // Generate a new id for the session being opened.
  const base::UnguessableToken capture_session_id =
      base::UnguessableToken::Create();

  DCHECK(sessions_.find(capture_session_id) == sessions_.end());
  std::ostringstream string_stream;
  string_stream << "VideoCaptureManager::Open, device.name = " << device.name
                << ", device.id = " << device.id
                << ", capture_session_id = " << capture_session_id;
  EmitLogMessage(string_stream.str(), 1);

  // We just save the stream info for processing later.
  sessions_[capture_session_id] = device;

  // Notify our listener asynchronously; this ensures that we return
  // |capture_session_id| to the caller of this function before using that
  // same id in a listener event.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&VideoCaptureManager::OnOpened, this,
                                device.type, capture_session_id));
  return capture_session_id;
}

void VideoCaptureManager::Close(
    const base::UnguessableToken& capture_session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "VideoCaptureManager::Close");

  std::ostringstream string_stream;
  string_stream << "VideoCaptureManager::Close, capture_session_id = "
                << capture_session_id;
  EmitLogMessage(string_stream.str(), 1);

  auto session_it = sessions_.find(capture_session_id);
  if (session_it == sessions_.end()) {
    return;
  }

  VideoCaptureController* const existing_device =
      LookupControllerByMediaTypeAndDeviceId(session_it->second.type,
                                             session_it->second.id);
  if (existing_device) {
    // Remove any client that is still using the session. This is safe to call
    // even if there are no clients using the session.
    existing_device->StopSession(capture_session_id);

    // StopSession() may have removed the last client, so we might need to
    // close the device.
    DestroyControllerIfNoClients(capture_session_id, existing_device);
  }

  // Notify listeners asynchronously, and forget the session.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&VideoCaptureManager::OnClosed, this,
                                session_it->second.type, capture_session_id));

  if (blink::IsDeviceMediaType(session_it->second.type)) {
    auto locked_it = locked_sessions_.find(session_it->first);
    const bool was_locked = locked_it != locked_sessions_.end();
    if (was_locked)
      locked_sessions_.erase(locked_it);
    if (locked_sessions_.empty() && !lock_time_.is_null()) {
      lock_time_ = base::TimeTicks();
      idle_close_timer_.Stop();
    }
  } else {
    DCHECK(!locked_sessions_.contains(session_it->first));
  }
  sessions_.erase(session_it);
}

void VideoCaptureManager::ApplySubCaptureTarget(
    const base::UnguessableToken& session_id,
    media::mojom::SubCaptureTargetType type,
    const base::Token& target,
    uint32_t sub_capture_target_version,
    base::OnceCallback<void(media::mojom::ApplySubCaptureTargetResult)>
        callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  VideoCaptureController* const controller =
      LookupControllerBySessionId(session_id);
  if (!controller || !controller->IsDeviceAlive()) {
    std::move(callback).Run(
        media::mojom::ApplySubCaptureTargetResult::kErrorGeneric);
    return;
  }
  controller->ApplySubCaptureTarget(type, target, sub_capture_target_version,
                                    std::move(callback));
}

void VideoCaptureManager::QueueStartDevice(
    const media::VideoCaptureSessionId& session_id,
    VideoCaptureController* controller,
    const media::VideoCaptureParams& params,
    mojo::PendingRemote<video_effects::mojom::VideoEffectsProcessor>
        video_effects_processor) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(lock_time_.is_null());
  device_start_request_queue_.push_back(CaptureDeviceStartRequest(
      controller, session_id, params, std::move(video_effects_processor)));
  if (device_start_request_queue_.size() == 1)
    ProcessDeviceStartRequestQueue();
}

void VideoCaptureManager::DoStopDevice(VideoCaptureController* controller) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "VideoCaptureManager::DoStopDevice");
  DCHECK(base::Contains(controllers_, controller));

  // If start request has not yet started processing, i.e. if it is not at the
  // beginning of the queue, remove it from the queue.
  if (!device_start_request_queue_.empty()) {
    auto second_request = std::next(device_start_request_queue_.begin());

    for (auto it = second_request; it != device_start_request_queue_.end();) {
      if (it->controller() == controller)
        it = device_start_request_queue_.erase(it);
      else
        ++it;
    }
  }

  const media::VideoCaptureDeviceInfo* device_info =
      GetDeviceInfoById(controller->device_id());
  if (device_info != nullptr) {
    for (auto& observer : capture_observers_)
      observer.OnVideoCaptureStopped(device_info->descriptor.facing);
  }

  // Since we may be removing |controller| from |controllers_| while
  // ReleaseDeviceAsnyc() is executing, we pass it shared ownership to
  // |controller|.
  controller->ReleaseDeviceAsync(
      base::DoNothingWithBoundArgs(GetControllerSharedRef(controller)));
}

void VideoCaptureManager::ProcessDeviceStartRequestQueue() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "VideoCaptureManager::ProcessDeviceStartRequestQueue");
  auto request = device_start_request_queue_.begin();
  if (request == device_start_request_queue_.end())
    return;

  VideoCaptureController* const controller = request->controller();

  EmitLogMessage("VideoCaptureManager::ProcessDeviceStartRequestQueue", 3);
  // The unit test VideoCaptureManagerTest.OpenNotExisting requires us to fail
  // synchronously if the stream_type is MEDIA_DEVICE_VIDEO_CAPTURE and no
  // DeviceInfo matching the requested id is present (which is the case when
  // requesting a device with a bogus id). Note, that since other types of
  // failure during startup of the device are allowed to be reported
  // asynchronously, this requirement is questionable.
  // TODO(chfremer): Check if any production code actually depends on this
  // requirement. If not, relax the requirement in the test and remove the below
  // if block. See crbug.com/708251
  if (controller->stream_type() ==
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE) {
    const media::VideoCaptureDeviceInfo* device_info =
        GetDeviceInfoById(controller->device_id());
    if (!device_info) {
      OnDeviceLaunchFailed(
          controller,
          media::VideoCaptureError::
              kVideoCaptureManagerProcessDeviceStartQueueDeviceInfoNotFound);
      return;
    }
    for (auto& observer : capture_observers_)
      observer.OnVideoCaptureStarted(device_info->descriptor.facing);
  }

  // The method CreateAndStartDeviceAsync() is going to run asynchronously.
  // Since we may be removing the controller while it is executing, we need to
  // pass it shared ownership to itself so that it stays alive while executing.
  // And since the execution may make callbacks into |this|, we also need
  // to pass it shared ownership to |this|.
  // TODO(chfremer): Check if request->params() can actually be different from
  // controller->parameters, and simplify if this is not the case.
  controller->CreateAndStartDeviceAsync(
      request->params(), static_cast<VideoCaptureDeviceLaunchObserver*>(this),
      base::BindOnce([](scoped_refptr<VideoCaptureManager>,
                        scoped_refptr<VideoCaptureController>) {},
                     scoped_refptr<VideoCaptureManager>(this),
                     GetControllerSharedRef(controller)),
      request->TakeVideoEffectsProcessor());
}

void VideoCaptureManager::OnDeviceLaunched(VideoCaptureController* controller) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  std::ostringstream string_stream;
  string_stream << "Launching device has succeeded. device_id = "
                << controller->device_id();
  EmitLogMessage(string_stream.str(), 1);
  DCHECK(!device_start_request_queue_.empty());
  DCHECK_EQ(controller, device_start_request_queue_.begin()->controller());
  DCHECK(controller);

  if (blink::IsVideoDesktopCaptureMediaType(controller->stream_type())) {
    const media::VideoCaptureSessionId session_id =
        device_start_request_queue_.front().session_id();
    DCHECK_NE(session_id, FakeSessionId());
    MaybePostDesktopCaptureWindowId(session_id);
  }

  auto it = photo_request_queue_.begin();
  while (it != photo_request_queue_.end()) {
    VideoCaptureController* maybe_entry =
        LookupControllerBySessionId(it->first);
    if (maybe_entry && maybe_entry->IsDeviceAlive()) {
      std::move(it->second).Run();
      it = photo_request_queue_.erase(it);
    } else {
      ++it;
    }
  }

  device_start_request_queue_.pop_front();
  ProcessDeviceStartRequestQueue();
}

void VideoCaptureManager::OnDeviceLaunchFailed(
    VideoCaptureController* controller,
    media::VideoCaptureError error) {
  std::ostringstream string_stream;
  string_stream << "Launching device has failed. device_id = "
                << controller->device_id();
  EmitLogMessage(string_stream.str(), 1);
  controller->OnError(error);

  device_start_request_queue_.pop_front();
  ProcessDeviceStartRequestQueue();
}

void VideoCaptureManager::OnDeviceLaunchAborted() {
  EmitLogMessage("Launching device has been aborted.", 1);
  device_start_request_queue_.pop_front();
  ProcessDeviceStartRequestQueue();
}

void VideoCaptureManager::OnDeviceConnectionLost(
    VideoCaptureController* controller) {
  std::ostringstream string_stream;
  string_stream << "Lost connection to device. device_id = "
                << controller->device_id();
  EmitLogMessage(string_stream.str(), 1);
  controller->OnError(
      media::VideoCaptureError::kVideoCaptureManagerDeviceConnectionLost);
}

void VideoCaptureManager::OpenNativeScreenCapturePicker(
    DesktopMediaID::Type type,
    base::OnceCallback<void(DesktopMediaID::Id)> created_callback,
    base::OnceCallback<void(webrtc::DesktopCapturer::Source)> picker_callback,
    base::OnceCallback<void()> cancel_callback,
    base::OnceCallback<void()> error_callback) {
  video_capture_provider_->OpenNativeScreenCapturePicker(
      type, std::move(created_callback), std::move(picker_callback),
      std::move(cancel_callback), std::move(error_callback));
}

void VideoCaptureManager::CloseNativeScreenCapturePicker(
    DesktopMediaID device_id) {
  video_capture_provider_->CloseNativeScreenCapturePicker(device_id);
}

void VideoCaptureManager::ConnectClient(
    const media::VideoCaptureSessionId& session_id,
    const media::VideoCaptureParams& params,
    VideoCaptureControllerID client_id,
    VideoCaptureControllerEventHandler* client_handler,
    std::optional<url::Origin> origin,
    DoneCB done_cb,
    BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "VideoCaptureManager::ConnectClient");
  {
    std::ostringstream string_stream;
    string_stream << "ConnectClient: session_id = " << session_id
                  << ", request: "
                  << media::VideoCaptureFormat::ToString(
                         params.requested_format);
    EmitLogMessage(string_stream.str(), 1);
  }

  VideoCaptureController* controller =
      GetOrCreateController(session_id, params);
  if (!controller) {
    std::move(done_cb).Run(nullptr);
    return;
  }

  bool client_exist =
      controller->HasActiveClient() || controller->HasPausedClient();
  base::UmaHistogramBoolean("Media.VideoCapture.StreamShared", client_exist);
  if (client_exist) {
    std::optional<url::Origin> first_client_origin =
        controller->GetFirstClientOrigin();
    bool same_origin = first_client_origin.has_value() && origin.has_value() &&
                       *first_client_origin == *origin;
    base::UmaHistogramBoolean("Media.VideoCapture.StreamSharedSameOrigin",
                              same_origin);
  }

  // First client starts the device. Device can't be started while the screen is
  // locked.
  if (!client_exist && lock_time_.is_null()) {
    std::ostringstream string_stream;
    string_stream
        << "VideoCaptureManager queueing device start for device_id = "
        << controller->device_id();
    EmitLogMessage(string_stream.str(), 1);
    mojo::PendingRemote<video_effects::mojom::VideoEffectsProcessor>
        video_effects_processor;
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_FUCHSIA)
    if (base::FeatureList::IsEnabled(media::kCameraMicEffects)) {
      auto* content_client = GetContentClient();
      if (browser_context && content_client && content_client->browser()) {
        content_client->browser()->BindVideoEffectsProcessor(
            controller->device_id(), browser_context,
            video_effects_processor.InitWithNewPipeAndPassReceiver());
      }
    }
#endif
    QueueStartDevice(session_id, controller, params,
                     std::move(video_effects_processor));
  }

  // Run the callback first, as AddClient() may trigger OnFrameInfo().
  std::move(done_cb).Run(controller->GetWeakPtrForIOThread());
  controller->AddClient(client_id, client_handler, session_id, params, origin);
}

void VideoCaptureManager::DisconnectClient(
    VideoCaptureController* controller,
    VideoCaptureControllerID client_id,
    VideoCaptureControllerEventHandler* client_handler,
    media::VideoCaptureError error) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "VideoCaptureManager::DisconnectClient");
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(controller);
  DCHECK(client_handler);
  CHECK(IsControllerPointerValid(controller));

  if (error != media::VideoCaptureError::kNone) {
    LogVideoCaptureError(error);
    std::ostringstream string_stream;
    string_stream << "Video capture session stopped with error code "
                  << static_cast<int>(error);
    EmitLogMessage(string_stream.str(), 1);
    for (auto it : sessions_) {
      if (it.second.type == controller->stream_type() &&
          it.second.id == controller->device_id()) {
        for (auto& listener : listeners_)
          listener.Aborted(it.second.type, it.first);
        // Aborted() call might synchronously destroy |controller|, recheck.
        if (!IsControllerPointerValid(controller))
          return;
        break;
      }
    }
  }

  // Detach client from controller.
  const media::VideoCaptureSessionId session_id =
      controller->RemoveClient(client_id, client_handler);
  std::ostringstream string_stream;
  string_stream << "DisconnectClient: session_id = " << session_id;
  EmitLogMessage(string_stream.str(), 1);

  // If controller has no more clients, delete controller and device.
  DestroyControllerIfNoClients(session_id, controller);
}

void VideoCaptureManager::PauseCaptureForClient(
    VideoCaptureController* controller,
    VideoCaptureControllerID client_id,
    VideoCaptureControllerEventHandler* client_handler) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(controller);
  DCHECK(client_handler);
  if (!IsControllerPointerValid(controller))
    NOTREACHED_IN_MIGRATION() << "Got Null controller while pausing capture";

  const bool had_active_client = controller->HasActiveClient();
  controller->PauseClient(client_id, client_handler);
  if (!had_active_client || controller->HasActiveClient())
    return;
  if (!controller->IsDeviceAlive())
    return;
  controller->MaybeSuspend();
}

void VideoCaptureManager::ResumeCaptureForClient(
    const media::VideoCaptureSessionId& session_id,
    const media::VideoCaptureParams& params,
    VideoCaptureController* controller,
    VideoCaptureControllerID client_id,
    VideoCaptureControllerEventHandler* client_handler) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(controller);
  DCHECK(client_handler);

  if (!IsControllerPointerValid(controller))
    NOTREACHED_IN_MIGRATION() << "Got Null controller while resuming capture";

  if (!controller->ResumeClient(client_id, client_handler)) {
    return;
  }
  if (!controller->IsDeviceAlive())
    return;
  controller->Resume();
}

void VideoCaptureManager::RequestRefreshFrameForClient(
    VideoCaptureController* controller) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (IsControllerPointerValid(controller)) {
    if (!controller->IsDeviceAlive())
      return;
    controller->RequestRefreshFrame();
  }
}

bool VideoCaptureManager::GetDeviceSupportedFormats(
    const media::VideoCaptureSessionId& capture_session_id,
    media::VideoCaptureFormats* supported_formats) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(supported_formats->empty());

  auto it = sessions_.find(capture_session_id);
  if (it == sessions_.end())
    return false;
  std::ostringstream string_stream;
  string_stream << "GetDeviceSupportedFormats for device: " << it->second.name;
  EmitLogMessage(string_stream.str(), 1);

  return GetDeviceSupportedFormats(it->second.id, supported_formats);
}

bool VideoCaptureManager::GetDeviceSupportedFormats(
    const std::string& device_id,
    media::VideoCaptureFormats* supported_formats) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(supported_formats->empty());

  // Return all available formats of the device, regardless its started state.
  media::VideoCaptureDeviceInfo* existing_device = GetDeviceInfoById(device_id);
  if (existing_device)
    *supported_formats = existing_device->supported_formats;
  return true;
}

bool VideoCaptureManager::GetDeviceFormatsInUse(
    const media::VideoCaptureSessionId& capture_session_id,
    media::VideoCaptureFormats* formats_in_use) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(formats_in_use->empty());

  auto it = sessions_.find(capture_session_id);
  if (it == sessions_.end())
    return false;
  std::ostringstream string_stream;
  string_stream << "GetDeviceFormatsInUse for device: " << it->second.name;
  EmitLogMessage(string_stream.str(), 1);

  std::optional<media::VideoCaptureFormat> format =
      GetDeviceFormatInUse(it->second.type, it->second.id);
  if (format.has_value())
    formats_in_use->push_back(format.value());

  return true;
}

std::optional<media::VideoCaptureFormat>
VideoCaptureManager::GetDeviceFormatInUse(
    blink::mojom::MediaStreamType stream_type,
    const std::string& device_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Return the currently in-use format of the device, if it's started.
  VideoCaptureController* device_in_use =
      LookupControllerByMediaTypeAndDeviceId(stream_type, device_id);
  return device_in_use ? device_in_use->GetVideoCaptureFormat() : std::nullopt;
}

GlobalRenderFrameHostId VideoCaptureManager::GetGlobalRenderFrameHostId(
    const base::UnguessableToken& session_id) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  VideoCaptureController* const controller =
      LookupControllerBySessionId(session_id);
  if (!controller || !controller->IsDeviceAlive() ||
      !blink::IsVideoDesktopCaptureMediaType(controller->stream_type())) {
    return GlobalRenderFrameHostId();
  }

  const DesktopMediaID desktop_media_id =
      DesktopMediaID::Parse(controller->device_id());

  if (desktop_media_id.type != DesktopMediaID::Type::TYPE_WEB_CONTENTS ||
      desktop_media_id.web_contents_id.is_null()) {
    return GlobalRenderFrameHostId();
  }

  return GlobalRenderFrameHostId(
      desktop_media_id.web_contents_id.render_process_id,
      desktop_media_id.web_contents_id.main_render_frame_id);
}

void VideoCaptureManager::SetDesktopCaptureWindowId(
    const media::VideoCaptureSessionId& session_id,
    gfx::NativeViewId window_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  VLOG(2) << "SetDesktopCaptureWindowId called for session " << session_id;

  notification_window_ids_[session_id] = window_id;
  MaybePostDesktopCaptureWindowId(session_id);

  if (set_desktop_capture_window_id_callback_for_testing_) {
    set_desktop_capture_window_id_callback_for_testing_.Run(session_id,
                                                            window_id);
  }
}

void VideoCaptureManager::MaybePostDesktopCaptureWindowId(
    const media::VideoCaptureSessionId& session_id) {
  auto session_it = sessions_.find(session_id);
  if (session_it == sessions_.end())
    return;

  VideoCaptureController* const existing_device =
      LookupControllerByMediaTypeAndDeviceId(session_it->second.type,
                                             session_it->second.id);
  if (!existing_device) {
    DVLOG(2) << "Failed to find an existing screen capture device.";
    return;
  }

  if (!existing_device->IsDeviceAlive()) {
    DVLOG(2) << "Screen capture device not yet started.";
    return;
  }

  DCHECK(blink::IsVideoDesktopCaptureMediaType(existing_device->stream_type()));
  DesktopMediaID id = DesktopMediaID::Parse(existing_device->device_id());
  if (id.is_null())
    return;

  auto window_id_it = notification_window_ids_.find(session_id);
  if (window_id_it == notification_window_ids_.end()) {
    DVLOG(2) << "Notification window id not set for screen capture.";
    return;
  }

  existing_device->SetDesktopCaptureWindowIdAsync(
      window_id_it->second,
      base::DoNothingWithBoundArgs(scoped_refptr<VideoCaptureManager>(this)));
  notification_window_ids_.erase(window_id_it);
}

void VideoCaptureManager::GetPhotoState(
    const base::UnguessableToken& session_id,
    media::VideoCaptureDevice::GetPhotoStateCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  VideoCaptureController* controller = LookupControllerBySessionId(session_id);
  if (!controller)
    return;
  if (controller->IsDeviceAlive()) {
    controller->GetPhotoState(std::move(callback));
    return;
  }
  // Queue up a request for later.
  photo_request_queue_.emplace_back(
      session_id,
      base::BindOnce(&VideoCaptureController::GetPhotoState,
                     controller->GetWeakPtrForIOThread(), std::move(callback)));
}

void VideoCaptureManager::SetPhotoOptions(
    const base::UnguessableToken& session_id,
    media::mojom::PhotoSettingsPtr settings,
    media::VideoCaptureDevice::SetPhotoOptionsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  VideoCaptureController* controller = LookupControllerBySessionId(session_id);
  if (!controller)
    return;
  if (controller->IsDeviceAlive()) {
    controller->SetPhotoOptions(std::move(settings), std::move(callback));
    return;
  }
  // Queue up a request for later.
  photo_request_queue_.emplace_back(
      session_id, base::BindOnce(&VideoCaptureController::SetPhotoOptions,
                                 controller->GetWeakPtrForIOThread(),
                                 std::move(settings), std::move(callback)));
}

void VideoCaptureManager::TakePhoto(
    const base::UnguessableToken& session_id,
    media::VideoCaptureDevice::TakePhotoCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "VideoCaptureManager::TakePhoto");

  VideoCaptureController* controller = LookupControllerBySessionId(session_id);
  if (!controller)
    return;
  if (controller->IsDeviceAlive()) {
    controller->TakePhoto(std::move(callback));
    return;
  }
  // Queue up a request for later.
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "VideoCaptureManager::TakePhoto enqueuing request");
  photo_request_queue_.emplace_back(
      session_id,
      base::BindOnce(&VideoCaptureController::TakePhoto,
                     controller->GetWeakPtrForIOThread(), std::move(callback)));
}

void VideoCaptureManager::OnOpened(
    blink::mojom::MediaStreamType stream_type,
    const media::VideoCaptureSessionId& capture_session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  for (auto& listener : listeners_)
    listener.Opened(stream_type, capture_session_id);
}

void VideoCaptureManager::OnClosed(
    blink::mojom::MediaStreamType stream_type,
    const media::VideoCaptureSessionId& capture_session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  for (auto& listener : listeners_)
    listener.Closed(stream_type, capture_session_id);
}

void VideoCaptureManager::OnDeviceInfosReceived(
    base::ElapsedTimer timer,
    EnumerationCallback client_callback,
    media::mojom::DeviceEnumerationResult error_code,
    const std::vector<media::VideoCaptureDeviceInfo>& device_infos) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "VideoCaptureManager::OnDeviceInfosReceived");


  if (error_code != media::mojom::DeviceEnumerationResult::kSuccess) {
    EmitLogMessage(
        base::StringPrintf("VideoCaptureManager::OnDeviceInfosReceived: Failed "
                           "to list device infos with error_code %d",
                           static_cast<int>(error_code)),
        0);
    std::move(client_callback).Run(error_code, {});
    return;
  }

  devices_info_cache_ = device_infos;

  std::ostringstream string_stream;
  string_stream << "VideoCaptureManager::OnDeviceInfosReceived: Recevied "
                << device_infos.size() << " device infos.";
  for (const auto& entry : device_infos) {
    string_stream << std::endl
                  << "device_id: " << entry.descriptor.device_id
                  << ", display_name: " << entry.descriptor.display_name();
  }
  EmitLogMessage(string_stream.str(), 1);

  // Walk the |devices_info_cache_| and produce a
  // media::VideoCaptureDeviceDescriptors for |client_callback|.
  media::VideoCaptureDeviceDescriptors devices;
  std::vector<std::tuple<media::VideoCaptureDeviceDescriptor,
                         media::VideoCaptureFormats>>
      descriptors_and_formats;
  for (const auto& it : devices_info_cache_) {
    devices.emplace_back(it.descriptor);
    descriptors_and_formats.emplace_back(it.descriptor, it.supported_formats);
    MediaInternals::GetInstance()->UpdateVideoCaptureDeviceCapabilities(
        descriptors_and_formats);
  }

  std::move(client_callback).Run(error_code, devices);
}

void VideoCaptureManager::DestroyControllerIfNoClients(
    const base::UnguessableToken& capture_session_id,
    VideoCaptureController* controller) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Removal of the last client stops the device.
  if (!controller->HasActiveClient() && !controller->HasPausedClient()) {
    std::ostringstream string_stream;
    string_stream << "VideoCaptureManager stopping device (stream_type = "
                  << controller->stream_type()
                  << ", device_id = " << controller->device_id() << ")";
    EmitLogMessage(string_stream.str(), 1);

    // Close the native OS picker as the associated VideoCaptureDevice is being
    // closed.
    CloseNativeScreenCapturePicker(
        DesktopMediaID::Parse(controller->device_id()));

    // The VideoCaptureController is removed from |controllers_| immediately.
    // The controller is deleted immediately, and the device is freed
    // asynchronously. After this point, subsequent requests to open this same
    // device ID will create a new VideoCaptureController,
    // VideoCaptureController, and VideoCaptureDevice.
    DoStopDevice(controller);
    // TODO(mcasas): use a helper function https://crbug.com/624854.
    auto controller_iter = base::ranges::find(
        controllers_, controller, &scoped_refptr<VideoCaptureController>::get);
    controllers_.erase(controller_iter);
    // Check if there are any associated pending callbacks and delete them.
    base::EraseIf(photo_request_queue_,
                  [capture_session_id](const auto& request) {
                    return request.first == capture_session_id;
                  });
  }
}

VideoCaptureController* VideoCaptureManager::LookupControllerBySessionId(
    const base::UnguessableToken& session_id) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  SessionMap::const_iterator session_it = sessions_.find(session_id);
  if (session_it == sessions_.end())
    return nullptr;

  return LookupControllerByMediaTypeAndDeviceId(session_it->second.type,
                                                session_it->second.id);
}

VideoCaptureController*
VideoCaptureManager::LookupControllerByMediaTypeAndDeviceId(
    blink::mojom::MediaStreamType type,
    const std::string& device_id) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  for (const auto& entry : controllers_) {
    if (type == entry->stream_type() && device_id == entry->device_id())
      return entry.get();
  }
  return nullptr;
}

bool VideoCaptureManager::IsControllerPointerValid(
    const VideoCaptureController* controller) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return base::Contains(controllers_, controller);
}

scoped_refptr<VideoCaptureController>
VideoCaptureManager::GetControllerSharedRef(
    VideoCaptureController* controller) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  for (const auto& entry : controllers_) {
    if (entry.get() == controller)
      return entry;
  }
  return nullptr;
}

media::VideoCaptureDeviceInfo* VideoCaptureManager::GetDeviceInfoById(
    const std::string& id) {
  for (auto& it : devices_info_cache_) {
    if (it.descriptor.device_id == id)
      return &it;
  }
  return nullptr;
}

VideoCaptureController* VideoCaptureManager::GetOrCreateController(
    const media::VideoCaptureSessionId& capture_session_id,
    const media::VideoCaptureParams& params) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto session_it = sessions_.find(capture_session_id);
  if (session_it == sessions_.end())
    return nullptr;
  const blink::MediaStreamDevice& device_info = session_it->second;

  // Check if another session has already opened this device. If so, just
  // use that opened device.
  VideoCaptureController* const existing_device =
      LookupControllerByMediaTypeAndDeviceId(device_info.type, device_info.id);
  if (existing_device) {
    DCHECK_EQ(device_info.type, existing_device->stream_type());
    if (existing_device->was_crop_ever_called()) {
      return nullptr;
    }
    return existing_device;
  }

  VideoCaptureController* new_controller = new VideoCaptureController(
      device_info.id, device_info.type, params,
      video_capture_provider_->CreateDeviceLauncher(), emit_log_message_cb_);
  controllers_.emplace_back(new_controller);
  return new_controller;
}

#if BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_DESKTOP_ANDROID)
void VideoCaptureManager::OnApplicationStateChange(
    base::android::ApplicationState state) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Only release/resume devices when the Application state changes from
  // RUNNING->STOPPED->RUNNING.
  if (state == base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES &&
      !application_state_has_running_activities_) {
    ResumeDevices();
    application_state_has_running_activities_ = true;
  } else if (state == base::android::APPLICATION_STATE_HAS_STOPPED_ACTIVITIES) {
    ReleaseDevices();
    application_state_has_running_activities_ = false;
  }
}
#endif

void VideoCaptureManager::ReleaseDevices() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  for (auto& controller : controllers_) {
    // Do not stop Content Video Capture devices, e.g. Tab or Screen capture.
    if (controller->stream_type() !=
        blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE)
      continue;

    DoStopDevice(controller.get());
  }
}

void VideoCaptureManager::ResumeDevices() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  for (auto& controller : controllers_) {
    // Do not resume Content Video Capture devices, e.g. Tab or Screen capture.
    // Do not try to restart already running devices.
    if (controller->stream_type() !=
            blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE ||
        controller->IsDeviceAlive())
      continue;

    // Check if the device is already in the start queue.
    bool device_in_queue = false;
    for (auto& request : device_start_request_queue_) {
      if (request.controller() == controller.get()) {
        device_in_queue = true;
        break;
      }
    }

    if (!device_in_queue) {
      // Session ID is only valid for Screen capture. So we can fake it to
      // resume video capture devices here.
      QueueStartDevice(FakeSessionId(), controller.get(),
                       controller->parameters(), {});
    }
  }
}

void VideoCaptureManager::OnScreenLocked() {
#if !BUILDFLAG(IS_ANDROID)
  // Stop screen sharing when screen is locked on desktop platforms only.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  EmitLogMessage("VideoCaptureManager::OnScreenLocked", 1);

  std::vector<media::VideoCaptureSessionId> desktopcapture_session_ids;
  for (auto it : sessions_) {
    if (blink::IsDesktopCaptureMediaType(it.second.type))
      desktopcapture_session_ids.push_back(it.first);

    if (blink::IsDeviceMediaType(it.second.type))
      locked_sessions_.insert(it.first);
  }

  if (!locked_sessions_.empty()) {
    DCHECK(lock_time_.is_null());
    lock_time_ = base::TimeTicks::Now();

    idle_close_timer_.Start(FROM_HERE, idle_close_timeout_, this,
                            &VideoCaptureManager::ReleaseDevices);
  }

  for (auto session_id : desktopcapture_session_ids) {
    Close(session_id);
  }
#endif  // !BUILDFLAG(IS_ANDROID)
}

void VideoCaptureManager::OnScreenUnlocked() {
  EmitLogMessage("VideoCaptureManager::OnScreenUnlocked", 1);
  if (lock_time_.is_null())
    return;

  DCHECK(!locked_sessions_.empty());
  lock_time_ = base::TimeTicks();

  idle_close_timer_.Stop();
  ResumeDevices();
}

void VideoCaptureManager::EmitLogMessage(const std::string& message,
                                         int verbose_log_level) {
  DVLOG(verbose_log_level) << message;
  emit_log_message_cb_.Run(message);
}

}  // namespace content

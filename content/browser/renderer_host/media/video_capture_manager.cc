// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/video_capture_manager.h"

#include <algorithm>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/browser/media/media_internals.h"
#include "content/browser/renderer_host/media/video_capture_controller.h"
#include "content/browser/screenlock_monitor/screenlock_monitor.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/desktop_media_id.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/media_switches.h"
#include "media/base/video_facing.h"
#include "media/capture/video/video_capture_device.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"

namespace {

// Used for logging capture events.
// Elements in this enum should not be deleted or rearranged; the only
// permitted operation is to add new elements before NUM_VIDEO_CAPTURE_EVENT.
enum VideoCaptureEvent {
  VIDEO_CAPTURE_START_CAPTURE = 0,
  VIDEO_CAPTURE_STOP_CAPTURE_OK = 1,
  VIDEO_CAPTURE_STOP_CAPTURE_DUE_TO_ERROR = 2,
  VIDEO_CAPTURE_STOP_CAPTURE_OK_NO_FRAMES_PRODUCED_BY_DEVICE = 3,
  VIDEO_CAPTURE_STOP_CAPTURE_OK_NO_FRAMES_PRODUCED_BY_DESKTOP_OR_TAB = 4,
  NUM_VIDEO_CAPTURE_EVENT
};

void LogVideoCaptureEvent(VideoCaptureEvent event) {
  UMA_HISTOGRAM_ENUMERATION("Media.VideoCaptureManager.Event", event,
                            NUM_VIDEO_CAPTURE_EVENT);
}

void LogVideoCaptureError(media::VideoCaptureError error) {
  UMA_HISTOGRAM_ENUMERATION(
      "Media.VideoCapture.Error", error,
      static_cast<int>(media::VideoCaptureError::kMaxValue) + 1);
}

const base::UnguessableToken& FakeSessionId() {
  static const base::NoDestructor<base::UnguessableToken> fake_session_id(
      base::UnguessableToken::Deserialize(0xFFFFFFFFFFFFFFFFU,
                                          0xFFFFFFFFFFFFFFFFU));
  return *fake_session_id;
}

}  // namespace

namespace content {

// Class used for queuing request for starting a device.
class VideoCaptureManager::CaptureDeviceStartRequest {
 public:
  CaptureDeviceStartRequest(VideoCaptureController* controller,
                            const media::VideoCaptureSessionId& session_id,
                            const media::VideoCaptureParams& params);
  VideoCaptureController* controller() const { return controller_; }
  const base::UnguessableToken& session_id() const { return session_id_; }
  media::VideoCaptureParams params() const { return params_; }

 private:
  VideoCaptureController* const controller_;
  const base::UnguessableToken session_id_;
  const media::VideoCaptureParams params_;
};

VideoCaptureManager::CaptureDeviceStartRequest::CaptureDeviceStartRequest(
    VideoCaptureController* controller,
    const media::VideoCaptureSessionId& session_id,
    const media::VideoCaptureParams& params)
    : controller_(controller), session_id_(session_id), params_(params) {}

VideoCaptureManager::VideoCaptureManager(
    std::unique_ptr<VideoCaptureProvider> video_capture_provider,
    base::RepeatingCallback<void(const std::string&)> emit_log_message_cb,
    ScreenlockMonitor* monitor)
    : video_capture_provider_(std::move(video_capture_provider)),
      emit_log_message_cb_(std::move(emit_log_message_cb)),
      screenlock_monitor_(monitor) {
  if (screenlock_monitor_) {
    screenlock_monitor_->AddObserver(this);
  }
}

VideoCaptureManager::~VideoCaptureManager() {
  DCHECK(device_start_request_queue_.empty());
  if (screenlock_monitor_) {
    screenlock_monitor_->RemoveObserver(this);
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
#if defined(OS_ANDROID)
  application_state_has_running_activities_ = true;
  app_status_listener_ = base::android::ApplicationStatusListener::New(
      base::BindRepeating(&VideoCaptureManager::OnApplicationStateChange,
                          base::Unretained(this)));
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
  EmitLogMessage("VideoCaptureManager::EnumerateDevices", 1);

  // Pass a timer for UMA histogram collection.
  video_capture_provider_->GetDeviceInfosAsync(media::BindToCurrentLoop(
      base::BindRepeating(&VideoCaptureManager::OnDeviceInfosReceived, this,
                          base::Owned(new base::ElapsedTimer()),
                          base::Passed(&client_callback))));
}

base::UnguessableToken VideoCaptureManager::Open(
    const blink::MediaStreamDevice& device) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  TRACE_EVENT_INSTANT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
                       "VideoCaptureManager::Open", TRACE_EVENT_SCOPE_PROCESS);

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
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&VideoCaptureManager::OnOpened, this,
                                device.type, capture_session_id));
  return capture_session_id;
}

void VideoCaptureManager::Close(
    const base::UnguessableToken& capture_session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  TRACE_EVENT_INSTANT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
                       "VideoCaptureManager::Close", TRACE_EVENT_SCOPE_PROCESS);

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
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&VideoCaptureManager::OnClosed, this,
                                session_it->second.type, capture_session_id));
  sessions_.erase(session_it);
}

void VideoCaptureManager::QueueStartDevice(
    const media::VideoCaptureSessionId& session_id,
    VideoCaptureController* controller,
    const media::VideoCaptureParams& params) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  device_start_request_queue_.push_back(
      CaptureDeviceStartRequest(controller, session_id, params));
  if (device_start_request_queue_.size() == 1)
    ProcessDeviceStartRequestQueue();
}

void VideoCaptureManager::DoStopDevice(VideoCaptureController* controller) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  TRACE_EVENT_INSTANT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
                       "VideoCaptureManager::DoStopDevice",
                       TRACE_EVENT_SCOPE_PROCESS);
  // TODO(mcasas): use a helper function https://crbug.com/624854.
  DCHECK(std::find_if(
             controllers_.begin(), controllers_.end(),
             [controller](
                 const scoped_refptr<VideoCaptureController>& device_entry) {
               return device_entry.get() == controller;
             }) != controllers_.end());

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
      base::BindOnce([](scoped_refptr<VideoCaptureController>) {},
                     GetControllerSharedRef(controller)));
}

void VideoCaptureManager::ProcessDeviceStartRequestQueue() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  TRACE_EVENT_INSTANT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
                       "VideoCaptureManager::ProcessDeviceStartRequestQueue",
                       TRACE_EVENT_SCOPE_PROCESS);
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
                     GetControllerSharedRef(controller)));
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
    auto request = it++;
    VideoCaptureController* maybe_entry =
        LookupControllerBySessionId(request->first);
    if (maybe_entry && maybe_entry->IsDeviceAlive()) {
      request->second.Run();
      photo_request_queue_.erase(request);
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

void VideoCaptureManager::ConnectClient(
    const media::VideoCaptureSessionId& session_id,
    const media::VideoCaptureParams& params,
    VideoCaptureControllerID client_id,
    VideoCaptureControllerEventHandler* client_handler,
    const DoneCB& done_cb) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  TRACE_EVENT_INSTANT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
                       "VideoCaptureManager::ConnectClient",
                       TRACE_EVENT_SCOPE_PROCESS);
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
    done_cb.Run(base::WeakPtr<VideoCaptureController>());
    return;
  }

  LogVideoCaptureEvent(VIDEO_CAPTURE_START_CAPTURE);

  // First client starts the device.
  if (!controller->HasActiveClient() && !controller->HasPausedClient()) {
    std::ostringstream string_stream;
    string_stream
        << "VideoCaptureManager queueing device start for device_id = "
        << controller->device_id();
    EmitLogMessage(string_stream.str(), 1);
    QueueStartDevice(session_id, controller, params);
  }
  // Run the callback first, as AddClient() may trigger OnFrameInfo().
  done_cb.Run(controller->GetWeakPtrForIOThread());
  controller->AddClient(client_id, client_handler, session_id, params);
}

void VideoCaptureManager::DisconnectClient(
    VideoCaptureController* controller,
    VideoCaptureControllerID client_id,
    VideoCaptureControllerEventHandler* client_handler,
    media::VideoCaptureError error) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(controller);
  DCHECK(client_handler);
  TRACE_EVENT_INSTANT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
                       "VideoCaptureManager::DisconnectClient",
                       TRACE_EVENT_SCOPE_PROCESS);

  if (!IsControllerPointerValid(controller)) {
    NOTREACHED();
    return;
  }
  if (error == media::VideoCaptureError::kNone) {
    if (controller->has_received_frames()) {
      LogVideoCaptureEvent(VIDEO_CAPTURE_STOP_CAPTURE_OK);
    } else if (controller->stream_type() ==
               blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE) {
      LogVideoCaptureEvent(
          VIDEO_CAPTURE_STOP_CAPTURE_OK_NO_FRAMES_PRODUCED_BY_DEVICE);
    } else {
      LogVideoCaptureEvent(
          VIDEO_CAPTURE_STOP_CAPTURE_OK_NO_FRAMES_PRODUCED_BY_DESKTOP_OR_TAB);
    }
  } else {
    LogVideoCaptureEvent(VIDEO_CAPTURE_STOP_CAPTURE_DUE_TO_ERROR);
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
    NOTREACHED() << "Got Null controller while pausing capture";

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
    NOTREACHED() << "Got Null controller while resuming capture";

  const bool had_active_client = controller->HasActiveClient();
  controller->ResumeClient(client_id, client_handler);
  if (had_active_client || !controller->HasActiveClient())
    return;
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

  base::Optional<media::VideoCaptureFormat> format =
      GetDeviceFormatInUse(it->second.type, it->second.id);
  if (format.has_value())
    formats_in_use->push_back(format.value());

  return true;
}

base::Optional<media::VideoCaptureFormat>
VideoCaptureManager::GetDeviceFormatInUse(
    blink::mojom::MediaStreamType stream_type,
    const std::string& device_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Return the currently in-use format of the device, if it's started.
  VideoCaptureController* device_in_use =
      LookupControllerByMediaTypeAndDeviceId(stream_type, device_id);
  return device_in_use ? device_in_use->GetVideoCaptureFormat() : base::nullopt;
}

void VideoCaptureManager::SetDesktopCaptureWindowId(
    const media::VideoCaptureSessionId& session_id,
    gfx::NativeViewId window_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  VLOG(2) << "SetDesktopCaptureWindowId called for session " << session_id;

  notification_window_ids_[session_id] = window_id;
  MaybePostDesktopCaptureWindowId(session_id);
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
      base::BindOnce([](scoped_refptr<VideoCaptureManager>) {},
                     scoped_refptr<VideoCaptureManager>(this)));
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
      base::Bind(&VideoCaptureController::GetPhotoState,
                 controller->GetWeakPtrForIOThread(), base::Passed(&callback)));
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
      session_id, base::Bind(&VideoCaptureController::SetPhotoOptions,
                             controller->GetWeakPtrForIOThread(),
                             base::Passed(&settings), base::Passed(&callback)));
}

void VideoCaptureManager::TakePhoto(
    const base::UnguessableToken& session_id,
    media::VideoCaptureDevice::TakePhotoCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  TRACE_EVENT_INSTANT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
                       "VideoCaptureManager::TakePhoto",
                       TRACE_EVENT_SCOPE_PROCESS);

  VideoCaptureController* controller = LookupControllerBySessionId(session_id);
  if (!controller)
    return;
  if (controller->IsDeviceAlive()) {
    controller->TakePhoto(std::move(callback));
    return;
  }
  // Queue up a request for later.
  TRACE_EVENT_INSTANT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
                       "VideoCaptureManager::TakePhoto enqueuing request",
                       TRACE_EVENT_SCOPE_PROCESS);
  photo_request_queue_.emplace_back(
      session_id,
      base::Bind(&VideoCaptureController::TakePhoto,
                 controller->GetWeakPtrForIOThread(), base::Passed(&callback)));
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
    base::ElapsedTimer* timer,
    EnumerationCallback client_callback,
    const std::vector<media::VideoCaptureDeviceInfo>& device_infos) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  TRACE_EVENT_INSTANT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
                       "VideoCaptureManager::OnDeviceInfosReceived",
                       TRACE_EVENT_SCOPE_PROCESS);

  UMA_HISTOGRAM_TIMES(
      "Media.VideoCaptureManager.GetAvailableDevicesInfoOnDeviceThreadTime",
      timer->Elapsed());
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

  std::move(client_callback).Run(devices);
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

    // The VideoCaptureController is removed from |controllers_| immediately.
    // The controller is deleted immediately, and the device is freed
    // asynchronously. After this point, subsequent requests to open this same
    // device ID will create a new VideoCaptureController,
    // VideoCaptureController, and VideoCaptureDevice.
    DoStopDevice(controller);
    // TODO(mcasas): use a helper function https://crbug.com/624854.
    auto controller_iter = std::find_if(
        controllers_.begin(), controllers_.end(),
        [controller](
            const scoped_refptr<VideoCaptureController>& device_entry) {
          return device_entry.get() == controller;
        });
    controllers_.erase(controller_iter);
    // Check if there are any associated pending callbacks and delete them.
    photo_request_queue_.remove_if(
        [capture_session_id](
            const std::pair<base::UnguessableToken, base::Closure>& request) {
          return request.first == capture_session_id;
        });
  }
}

VideoCaptureController* VideoCaptureManager::LookupControllerBySessionId(
    const base::UnguessableToken& session_id) {
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
    return existing_device;
  }

  VideoCaptureController* new_controller = new VideoCaptureController(
      device_info.id, device_info.type, params,
      video_capture_provider_->CreateDeviceLauncher(), emit_log_message_cb_);
  controllers_.emplace_back(new_controller);
  return new_controller;
}

#if defined(OS_ANDROID)
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
                       controller->parameters());
    }
  }
}
#endif  // defined(OS_ANDROID)

void VideoCaptureManager::OnScreenLocked() {
#if !defined(OS_ANDROID)
  // Stop screen sharing when screen is locked on desktop platforms only.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  EmitLogMessage("VideoCaptureManager::OnScreenLocked", 1);

  std::vector<media::VideoCaptureSessionId> desktopcapture_session_ids;
  for (auto it : sessions_) {
    if (blink::IsDesktopCaptureMediaType(it.second.type))
      desktopcapture_session_ids.push_back(it.first);
  }

  for (auto session_id : desktopcapture_session_ids) {
    Close(session_id);
  }
#endif  // OS_ANDROID
}

void VideoCaptureManager::EmitLogMessage(const std::string& message,
                                         int verbose_log_level) {
  DVLOG(verbose_log_level) << message;
  emit_log_message_cb_.Run(message);
}

}  // namespace content

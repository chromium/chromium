// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/video_capture_controller.h"

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <set>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "media/base/video_frame.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "media/capture/video/video_capture_buffer_pool.h"
#include "media/capture/video/video_capture_buffer_tracker_factory_impl.h"
#include "media/capture/video/video_capture_device_client.h"
#include "mojo/public/cpp/system/platform_handle.h"

#if !defined(OS_ANDROID)
#include "content/browser/compositor/image_transport_factory.h"
#endif

using media::VideoCaptureFormat;
using media::VideoFrame;
using media::VideoFrameMetadata;

namespace content {

namespace {

// Counter used for identifying a DeviceRequest to start a capture device.
static int g_device_start_id = 0;

static const int kInfiniteRatio = 99999;

#define UMA_HISTOGRAM_ASPECT_RATIO(name, width, height) \
  base::UmaHistogramSparse(                             \
      name, (height) ? ((width)*100) / (height) : kInfiniteRatio);

void LogVideoFrameDrop(media::VideoCaptureFrameDropReason reason,
                       blink::mojom::MediaStreamType stream_type) {
  const int kEnumCount =
      static_cast<int>(media::VideoCaptureFrameDropReason::kMaxValue) + 1;
  UMA_HISTOGRAM_ENUMERATION("Media.VideoCapture.FrameDrop", reason, kEnumCount);
  switch (stream_type) {
    case blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE:
      UMA_HISTOGRAM_ENUMERATION("Media.VideoCapture.FrameDrop.DeviceCapture",
                                reason, kEnumCount);
      break;
    case blink::mojom::MediaStreamType::GUM_TAB_VIDEO_CAPTURE:
      UMA_HISTOGRAM_ENUMERATION("Media.VideoCapture.FrameDrop.GumTabCapture",
                                reason, kEnumCount);
      break;
    case blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE:
      UMA_HISTOGRAM_ENUMERATION(
          "Media.VideoCapture.FrameDrop.GumDesktopCapture", reason, kEnumCount);
      break;
    case blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE:
      UMA_HISTOGRAM_ENUMERATION("Media.VideoCapture.FrameDrop.DisplayCapture",
                                reason, kEnumCount);
      break;
    default:
      // Do nothing
      return;
  }
}

void LogMaxConsecutiveVideoFrameDropCountExceeded(
    media::VideoCaptureFrameDropReason reason,
    blink::mojom::MediaStreamType stream_type) {
  const int kEnumCount =
      static_cast<int>(media::VideoCaptureFrameDropReason::kMaxValue) + 1;
  UMA_HISTOGRAM_ENUMERATION("Media.VideoCapture.MaxFrameDropExceeded", reason,
                            kEnumCount);
  switch (stream_type) {
    case blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE:
      UMA_HISTOGRAM_ENUMERATION(
          "Media.VideoCapture.MaxFrameDropExceeded.DeviceCapture", reason,
          kEnumCount);
      break;
    case blink::mojom::MediaStreamType::GUM_TAB_VIDEO_CAPTURE:
      UMA_HISTOGRAM_ENUMERATION(
          "Media.VideoCapture.MaxFrameDropExceeded.GumTabCapture", reason,
          kEnumCount);
      break;
    case blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE:
      UMA_HISTOGRAM_ENUMERATION(
          "Media.VideoCapture.MaxFrameDropExceeded.GumDesktopCapture", reason,
          kEnumCount);
      break;
    case blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE:
      UMA_HISTOGRAM_ENUMERATION(
          "Media.VideoCapture.MaxFrameDropExceeded.DisplayCapture", reason,
          kEnumCount);
      break;
    default:
      // Do nothing
      return;
  }
}

void CallOnError(media::VideoCaptureError error,
                 VideoCaptureControllerEventHandler* client,
                 const VideoCaptureControllerID& id) {
  client->OnError(id, error);
}

void CallOnStarted(VideoCaptureControllerEventHandler* client,
                   const VideoCaptureControllerID& id) {
  client->OnStarted(id);
}

void CallOnStartedUsingGpuDecode(VideoCaptureControllerEventHandler* client,
                                 const VideoCaptureControllerID& id) {
  client->OnStartedUsingGpuDecode(id);
}

}  // anonymous namespace

struct VideoCaptureController::ControllerClient {
  ControllerClient(const VideoCaptureControllerID& id,
                   VideoCaptureControllerEventHandler* handler,
                   const media::VideoCaptureSessionId& session_id,
                   const media::VideoCaptureParams& params)
      : controller_id(id),
        event_handler(handler),
        session_id(session_id),
        parameters(params),
        session_closed(false),
        paused(false) {}

  ~ControllerClient() {}

  // ID used for identifying this object.
  const VideoCaptureControllerID controller_id;
  VideoCaptureControllerEventHandler* const event_handler;

  const media::VideoCaptureSessionId session_id;
  const media::VideoCaptureParams parameters;

  std::vector<int> known_buffer_context_ids;
  // |buffer_context_id|s of buffers currently being consumed by this client.
  std::vector<int> buffers_in_use;

  // State of capture session, controlled by VideoCaptureManager directly. This
  // transitions to true as soon as StopSession() occurs, at which point the
  // client is sent an OnEnded() event. However, because the client retains a
  // VideoCaptureController* pointer, its ControllerClient entry lives on until
  // it unregisters itself via RemoveClient(), which may happen asynchronously.
  //
  // TODO(nick): If we changed the semantics of VideoCaptureHost so that
  // OnEnded() events were processed synchronously (with the RemoveClient() done
  // implicitly), we could avoid tracking this state here in the Controller, and
  // simplify the code in both places.
  bool session_closed;

  // Indicates whether the client is paused, if true, VideoCaptureController
  // stops updating its buffer.
  bool paused;
};

VideoCaptureController::BufferContext::BufferContext(
    int buffer_context_id,
    int buffer_id,
    media::VideoFrameConsumerFeedbackObserver* consumer_feedback_observer,
    media::mojom::VideoBufferHandlePtr buffer_handle)
    : buffer_context_id_(buffer_context_id),
      buffer_id_(buffer_id),
      is_retired_(false),
      frame_feedback_id_(0),
      consumer_feedback_observer_(consumer_feedback_observer),
      buffer_handle_(std::move(buffer_handle)),
      max_consumer_utilization_(
          media::VideoFrameConsumerFeedbackObserver::kNoUtilizationRecorded),
      consumer_hold_count_(0) {}

VideoCaptureController::BufferContext::~BufferContext() = default;

VideoCaptureController::BufferContext::BufferContext(
    VideoCaptureController::BufferContext&& other) = default;

VideoCaptureController::BufferContext& VideoCaptureController::BufferContext::
operator=(BufferContext&& other) = default;

void VideoCaptureController::BufferContext::RecordConsumerUtilization(
    double utilization) {
  if (std::isfinite(utilization) && utilization >= 0.0) {
    max_consumer_utilization_ =
        std::max(max_consumer_utilization_, utilization);
  }
}

void VideoCaptureController::BufferContext::IncreaseConsumerCount() {
  consumer_hold_count_++;
}

void VideoCaptureController::BufferContext::DecreaseConsumerCount() {
  consumer_hold_count_--;
  if (consumer_hold_count_ == 0) {
    if (consumer_feedback_observer_ != nullptr &&
        max_consumer_utilization_ !=
            media::VideoFrameConsumerFeedbackObserver::kNoUtilizationRecorded) {
      consumer_feedback_observer_->OnUtilizationReport(
          frame_feedback_id_, max_consumer_utilization_);
    }
    buffer_read_permission_.reset();
    max_consumer_utilization_ =
        media::VideoFrameConsumerFeedbackObserver::kNoUtilizationRecorded;
  }
}

media::mojom::VideoBufferHandlePtr
VideoCaptureController::BufferContext::CloneBufferHandle() {
  // Unable to use buffer_handle_->Clone(), because shared_buffer does not
  // support the copy constructor.
  media::mojom::VideoBufferHandlePtr result =
      media::mojom::VideoBufferHandle::New();
  if (buffer_handle_->is_shared_buffer_handle()) {
    // Buffer handles are always writable as they come from
    // VideoCaptureBufferPool which, among other use cases, provides decoder
    // output buffers.
    //
    // TODO(crbug.com/793446): BroadcastingReceiver::BufferContext also defines
    // CloneBufferHandle and independently decides on handle permissions. The
    // permissions should be coordinated between these two classes.
    result->set_shared_buffer_handle(
        buffer_handle_->get_shared_buffer_handle()->Clone(
            mojo::SharedBufferHandle::AccessMode::READ_WRITE));
    DCHECK(result->get_shared_buffer_handle()->is_valid());
  } else if (buffer_handle_->is_read_only_shmem_region()) {
    result->set_read_only_shmem_region(
        buffer_handle_->get_read_only_shmem_region().Duplicate());
    DCHECK(result->get_read_only_shmem_region().IsValid());
  } else if (buffer_handle_->is_mailbox_handles()) {
    result->set_mailbox_handles(buffer_handle_->get_mailbox_handles()->Clone());
  } else if (buffer_handle_->is_gpu_memory_buffer_handle()) {
    result->set_gpu_memory_buffer_handle(
        buffer_handle_->get_gpu_memory_buffer_handle().Clone());
  } else {
    NOTREACHED() << "Unexpected video buffer handle type";
  }
  return result;
}

VideoCaptureController::FrameDropLogState::FrameDropLogState(
    media::VideoCaptureFrameDropReason reason)
    : drop_count((reason == media::VideoCaptureFrameDropReason::kNone) ? 0 : 1),
      drop_reason(reason),
      max_log_count_exceeded(false) {}

VideoCaptureController::VideoCaptureController(
    const std::string& device_id,
    blink::mojom::MediaStreamType stream_type,
    const media::VideoCaptureParams& params,
    std::unique_ptr<VideoCaptureDeviceLauncher> device_launcher,
    base::RepeatingCallback<void(const std::string&)> emit_log_message_cb)
    : serial_id_(g_device_start_id++),
      device_id_(device_id),
      stream_type_(stream_type),
      parameters_(params),
      device_launcher_(std::move(device_launcher)),
      emit_log_message_cb_(std::move(emit_log_message_cb)),
      device_launch_observer_(nullptr),
      state_(blink::VIDEO_CAPTURE_STATE_STARTING),
      has_received_frames_(false) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

VideoCaptureController::~VideoCaptureController() = default;

base::WeakPtr<VideoCaptureController>
VideoCaptureController::GetWeakPtrForIOThread() {
  return weak_ptr_factory_.GetWeakPtr();
}

void VideoCaptureController::AddClient(
    const VideoCaptureControllerID& id,
    VideoCaptureControllerEventHandler* event_handler,
    const media::VideoCaptureSessionId& session_id,
    const media::VideoCaptureParams& params) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  std::ostringstream string_stream;
  string_stream << "VideoCaptureController::AddClient(): id = " << id
                << ", session_id = " << session_id.ToString()
                << ", params.requested_format = "
                << media::VideoCaptureFormat::ToString(params.requested_format);
  EmitLogMessage(string_stream.str(), 1);

  // Check that requested VideoCaptureParams are valid and supported.  If not,
  // report an error immediately and punt.
  if (!params.IsValid() ||
      !(params.requested_format.pixel_format == media::PIXEL_FORMAT_I420 ||
        params.requested_format.pixel_format == media::PIXEL_FORMAT_Y16 ||
        params.requested_format.pixel_format == media::PIXEL_FORMAT_ARGB)) {
    // Crash in debug builds since the renderer should not have asked for
    // invalid or unsupported parameters.
    LOG(DFATAL) << "Invalid or unsupported video capture parameters requested: "
                << media::VideoCaptureFormat::ToString(params.requested_format);
    event_handler->OnError(
        id,
        media::VideoCaptureError::
            kVideoCaptureControllerInvalidOrUnsupportedVideoCaptureParametersRequested);
    return;
  }

  // If this is the first client added to the controller, cache the parameters.
  if (controller_clients_.empty())
    video_capture_format_ = params.requested_format;

  // Signal error in case device is already in error state.
  if (state_ == blink::VIDEO_CAPTURE_STATE_ERROR) {
    event_handler->OnError(
        id,
        media::VideoCaptureError::kVideoCaptureControllerIsAlreadyInErrorState);
    return;
  }

  // Do nothing if this client has called AddClient before.
  if (FindClient(id, event_handler, controller_clients_))
    return;

  // If the device has reported OnStarted event, report it to this client here.
  if (state_ == blink::VIDEO_CAPTURE_STATE_STARTED)
    event_handler->OnStarted(id);

  std::unique_ptr<ControllerClient> client =
      std::make_unique<ControllerClient>(id, event_handler, session_id, params);
  // If we already have gotten frame_info from the device, repeat it to the new
  // client.
  if (state_ != blink::VIDEO_CAPTURE_STATE_ERROR) {
    controller_clients_.push_back(std::move(client));
  }
}

base::UnguessableToken VideoCaptureController::RemoveClient(
    const VideoCaptureControllerID& id,
    VideoCaptureControllerEventHandler* event_handler) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  std::ostringstream string_stream;
  string_stream << "VideoCaptureController::RemoveClient: id = " << id;
  EmitLogMessage(string_stream.str(), 1);

  ControllerClient* client = FindClient(id, event_handler, controller_clients_);
  if (!client)
    return base::UnguessableToken();

  for (const auto& buffer_id : client->buffers_in_use) {
    OnClientFinishedConsumingBuffer(
        client, buffer_id,
        media::VideoFrameConsumerFeedbackObserver::kNoUtilizationRecorded);
  }
  client->buffers_in_use.clear();

  base::UnguessableToken session_id = client->session_id;
  controller_clients_.remove_if(
      [client](const std::unique_ptr<ControllerClient>& ptr) {
        return ptr.get() == client;
      });

  return session_id;
}

void VideoCaptureController::PauseClient(
    const VideoCaptureControllerID& id,
    VideoCaptureControllerEventHandler* event_handler) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "VideoCaptureController::PauseClient: id = " << id;

  ControllerClient* client = FindClient(id, event_handler, controller_clients_);
  if (!client)
    return;

  DLOG_IF(WARNING, client->paused) << "Redundant client configuration";

  client->paused = true;
}

bool VideoCaptureController::ResumeClient(
    const VideoCaptureControllerID& id,
    VideoCaptureControllerEventHandler* event_handler) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "VideoCaptureController::ResumeClient: id = " << id;

  ControllerClient* client = FindClient(id, event_handler, controller_clients_);
  if (!client)
    return false;

  if (!client->paused) {
    DVLOG(1) << "Calling resume on unpaused client";
    return false;
  }

  client->paused = false;
  return true;
}

size_t VideoCaptureController::GetClientCount() const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return controller_clients_.size();
}

bool VideoCaptureController::HasActiveClient() const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  for (const auto& client : controller_clients_) {
    if (!client->paused)
      return true;
  }
  return false;
}

bool VideoCaptureController::HasPausedClient() const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  for (const auto& client : controller_clients_) {
    if (client->paused)
      return true;
  }
  return false;
}

void VideoCaptureController::StopSession(
    const base::UnguessableToken& session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  std::ostringstream string_stream;
  string_stream << "VideoCaptureController::StopSession: session_id = "
                << session_id;
  EmitLogMessage(string_stream.str(), 1);

  ControllerClient* client = FindClient(session_id, controller_clients_);

  if (client) {
    client->session_closed = true;
    client->event_handler->OnEnded(client->controller_id);
  }
}

void VideoCaptureController::ReturnBuffer(
    const VideoCaptureControllerID& id,
    VideoCaptureControllerEventHandler* event_handler,
    int buffer_id,
    double consumer_resource_utilization) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  ControllerClient* client = FindClient(id, event_handler, controller_clients_);

  // If this buffer is not held by this client, or this client doesn't exist
  // in controller, do nothing.
  if (!client) {
    NOTREACHED();
    return;
  }
  auto buffers_in_use_entry_iter =
      std::find(std::begin(client->buffers_in_use),
                std::end(client->buffers_in_use), buffer_id);
  if (buffers_in_use_entry_iter == std::end(client->buffers_in_use)) {
    NOTREACHED();
    return;
  }
  client->buffers_in_use.erase(buffers_in_use_entry_iter);

  OnClientFinishedConsumingBuffer(client, buffer_id,
                                  consumer_resource_utilization);
}

const base::Optional<media::VideoCaptureFormat>
VideoCaptureController::GetVideoCaptureFormat() const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return video_capture_format_;
}

void VideoCaptureController::OnNewBuffer(
    int32_t buffer_id,
    media::mojom::VideoBufferHandlePtr buffer_handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(FindUnretiredBufferContextFromBufferId(buffer_id) ==
         buffer_contexts_.end());
  buffer_contexts_.emplace_back(next_buffer_context_id_++, buffer_id,
                                launched_device_.get(),
                                std::move(buffer_handle));
}

void VideoCaptureController::OnFrameReadyInBuffer(
    int buffer_id,
    int frame_feedback_id,
    std::unique_ptr<
        media::VideoCaptureDevice::Client::Buffer::ScopedAccessPermission>
        buffer_read_permission,
    media::mojom::VideoFrameInfoPtr frame_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_NE(buffer_id, media::VideoCaptureBufferPool::kInvalidId);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "VideoCaptureController::OnFrameReadyInBuffer");

  frame_drop_log_state_ = FrameDropLogState();

  auto buffer_context_iter = FindUnretiredBufferContextFromBufferId(buffer_id);
  DCHECK(buffer_context_iter != buffer_contexts_.end());
  buffer_context_iter->set_frame_feedback_id(frame_feedback_id);
  DCHECK(!buffer_context_iter->HasConsumers());

  if (state_ != blink::VIDEO_CAPTURE_STATE_ERROR) {
    const int buffer_context_id = buffer_context_iter->buffer_context_id();
    for (const auto& client : controller_clients_) {
      if (client->session_closed || client->paused)
        continue;

      // On the first use of a BufferContext for a particular client, call
      // OnBufferCreated().
      if (!base::Contains(client->known_buffer_context_ids,
                          buffer_context_id)) {
        client->known_buffer_context_ids.push_back(buffer_context_id);
        client->event_handler->OnNewBuffer(
            client->controller_id, buffer_context_iter->CloneBufferHandle(),
            buffer_context_id);
      }

      if (!base::Contains(client->buffers_in_use, buffer_context_id))
        client->buffers_in_use.push_back(buffer_context_id);
      else
        NOTREACHED() << "Unexpected duplicate buffer: " << buffer_context_id;

      buffer_context_iter->IncreaseConsumerCount();
      client->event_handler->OnBufferReady(client->controller_id,
                                           buffer_context_id, frame_info);
    }
    if (buffer_context_iter->HasConsumers()) {
      buffer_context_iter->set_read_permission(
          std::move(buffer_read_permission));
    }
  }

  if (!has_received_frames_) {
    UMA_HISTOGRAM_COUNTS_1M("Media.VideoCapture.Width",
                            frame_info->coded_size.width());
    UMA_HISTOGRAM_COUNTS_1M("Media.VideoCapture.Height",
                            frame_info->coded_size.height());
    UMA_HISTOGRAM_ASPECT_RATIO("Media.VideoCapture.AspectRatio",
                               frame_info->coded_size.width(),
                               frame_info->coded_size.height());
    double frame_rate = 0.0f;
    if (video_capture_format_) {
      media::VideoFrameMetadata metadata;
      metadata.MergeInternalValuesFrom(frame_info->metadata);
      if (!metadata.GetDouble(VideoFrameMetadata::FRAME_RATE, &frame_rate)) {
        frame_rate = video_capture_format_->frame_rate;
      }
    }
    UMA_HISTOGRAM_COUNTS_1M("Media.VideoCapture.FrameRate", frame_rate);
    UMA_HISTOGRAM_TIMES("Media.VideoCapture.DelayUntilFirstFrame",
                        base::TimeTicks::Now() - time_of_start_request_);
    OnLog("First frame received at VideoCaptureController");
    has_received_frames_ = true;
  }
}

void VideoCaptureController::OnBufferRetired(int buffer_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto buffer_context_iter = FindUnretiredBufferContextFromBufferId(buffer_id);
  DCHECK(buffer_context_iter != buffer_contexts_.end());

  // If there are any clients still using the buffer, we need to allow them
  // to finish up. We need to hold on to the BufferContext entry until then,
  // because it contains the consumer hold.
  if (!buffer_context_iter->HasConsumers())
    ReleaseBufferContext(buffer_context_iter);
  else
    buffer_context_iter->set_is_retired();
}

void VideoCaptureController::OnError(media::VideoCaptureError error) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  state_ = blink::VIDEO_CAPTURE_STATE_ERROR;
  PerformForClientsWithOpenSession(base::BindRepeating(&CallOnError, error));
}

void VideoCaptureController::OnFrameDropped(
    media::VideoCaptureFrameDropReason reason) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "VideoCaptureController::OnFrameDropped");
  if (reason == frame_drop_log_state_.drop_reason) {
    if (frame_drop_log_state_.max_log_count_exceeded)
      return;

    if (++frame_drop_log_state_.drop_count >
        kMaxConsecutiveFrameDropForSameReasonCount) {
      frame_drop_log_state_.max_log_count_exceeded = true;
      LogMaxConsecutiveVideoFrameDropCountExceeded(reason, stream_type_);
      std::ostringstream string_stream;
      string_stream << "Too many consecutive frames dropped with reason code "
                    << static_cast<int>(reason)
                    << ". Stopping to log dropped frames for this reason in "
                       "order to avoid log spam.";
      EmitLogMessage(string_stream.str(), 1);
      return;
    }
  } else {
    frame_drop_log_state_ = FrameDropLogState(reason);
  }

  LogVideoFrameDrop(reason, stream_type_);
  std::ostringstream string_stream;
  string_stream << "Frame dropped with reason code "
                << static_cast<int>(reason);
  EmitLogMessage(string_stream.str(), 1);
}

void VideoCaptureController::OnLog(const std::string& message) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  EmitLogMessage(message, 3);
}

void VideoCaptureController::OnStarted() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  EmitLogMessage(__func__, 3);
  state_ = blink::VIDEO_CAPTURE_STATE_STARTED;
  PerformForClientsWithOpenSession(base::BindRepeating(&CallOnStarted));
}

void VideoCaptureController::OnStartedUsingGpuDecode() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  EmitLogMessage(__func__, 3);
  PerformForClientsWithOpenSession(
      base::BindRepeating(&CallOnStartedUsingGpuDecode));
}

void VideoCaptureController::OnStopped() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  EmitLogMessage(__func__, 3);
  // Clients of VideoCaptureController are currently not interested in
  // OnStopped events, so we simply swallow the event here. Note that, if we
  // wanted to forward it to clients in the future, care would have to be taken
  // for the case of there being outstanding OnBufferRetired() events that have
  // been deferred because a client was still consuming a retired buffer.
}

void VideoCaptureController::OnDeviceLaunched(
    std::unique_ptr<LaunchedVideoCaptureDevice> device) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  EmitLogMessage(__func__, 3);
  launched_device_ = std::move(device);
  for (auto& entry : buffer_contexts_)
    entry.set_consumer_feedback_observer(launched_device_.get());
  if (device_launch_observer_) {
    device_launch_observer_->OnDeviceLaunched(this);
  }
}

void VideoCaptureController::OnDeviceLaunchFailed(
    media::VideoCaptureError error) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  EmitLogMessage(__func__, 3);
  if (device_launch_observer_) {
    device_launch_observer_->OnDeviceLaunchFailed(this, error);
    device_launch_observer_ = nullptr;
  }
}

void VideoCaptureController::OnDeviceLaunchAborted() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  EmitLogMessage(__func__, 3);
  if (device_launch_observer_) {
    device_launch_observer_->OnDeviceLaunchAborted();
    device_launch_observer_ = nullptr;
  }
}

void VideoCaptureController::OnDeviceConnectionLost() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  EmitLogMessage(__func__, 3);
  if (device_launch_observer_) {
    device_launch_observer_->OnDeviceConnectionLost(this);
    device_launch_observer_ = nullptr;
  }
}

void VideoCaptureController::CreateAndStartDeviceAsync(
    const media::VideoCaptureParams& params,
    VideoCaptureDeviceLaunchObserver* observer,
    base::OnceClosure done_cb) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "VideoCaptureController::CreateAndStartDeviceAsync");
  std::ostringstream string_stream;
  string_stream
      << "VideoCaptureController::CreateAndStartDeviceAsync: serial_id = "
      << serial_id() << ", device_id = " << device_id();
  EmitLogMessage(string_stream.str(), 1);
  time_of_start_request_ = base::TimeTicks::Now();
  device_launch_observer_ = observer;
  device_launcher_->LaunchDeviceAsync(
      device_id_, stream_type_, params, GetWeakPtrForIOThread(),
      base::BindOnce(&VideoCaptureController::OnDeviceConnectionLost,
                     GetWeakPtrForIOThread()),
      this, std::move(done_cb));
}

void VideoCaptureController::ReleaseDeviceAsync(base::OnceClosure done_cb) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "VideoCaptureController::ReleaseDeviceAsync");
  std::ostringstream string_stream;
  string_stream << "VideoCaptureController::ReleaseDeviceAsync: serial_id = "
                << serial_id() << ", device_id = " << device_id();
  EmitLogMessage(string_stream.str(), 1);
  if (!launched_device_) {
    device_launcher_->AbortLaunch();
    return;
  }
  launched_device_.reset();
}

bool VideoCaptureController::IsDeviceAlive() const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return launched_device_ != nullptr;
}

void VideoCaptureController::GetPhotoState(
    media::VideoCaptureDevice::GetPhotoStateCallback callback) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(launched_device_);
  launched_device_->GetPhotoState(std::move(callback));
}

void VideoCaptureController::SetPhotoOptions(
    media::mojom::PhotoSettingsPtr settings,
    media::VideoCaptureDevice::SetPhotoOptionsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(launched_device_);
  launched_device_->SetPhotoOptions(std::move(settings), std::move(callback));
}

void VideoCaptureController::TakePhoto(
    media::VideoCaptureDevice::TakePhotoCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(launched_device_);
  TRACE_EVENT_INSTANT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
                       "VideoCaptureController::TakePhoto",
                       TRACE_EVENT_SCOPE_PROCESS);
  launched_device_->TakePhoto(std::move(callback));
}

void VideoCaptureController::MaybeSuspend() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(launched_device_);
  EmitLogMessage(__func__, 3);
  launched_device_->MaybeSuspendDevice();
}

void VideoCaptureController::Resume() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(launched_device_);
  EmitLogMessage(__func__, 3);
  launched_device_->ResumeDevice();
}

void VideoCaptureController::RequestRefreshFrame() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(launched_device_);
  launched_device_->RequestRefreshFrame();
}

void VideoCaptureController::SetDesktopCaptureWindowIdAsync(
    gfx::NativeViewId window_id,
    base::OnceClosure done_cb) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(launched_device_);
  launched_device_->SetDesktopCaptureWindowIdAsync(window_id,
                                                   std::move(done_cb));
}

VideoCaptureController::ControllerClient* VideoCaptureController::FindClient(
    const VideoCaptureControllerID& id,
    VideoCaptureControllerEventHandler* handler,
    const ControllerClients& clients) {
  for (const auto& client : clients) {
    if (client->controller_id == id && client->event_handler == handler)
      return client.get();
  }
  return nullptr;
}

VideoCaptureController::ControllerClient* VideoCaptureController::FindClient(
    const base::UnguessableToken& session_id,
    const ControllerClients& clients) {
  for (const auto& client : clients) {
    if (client->session_id == session_id)
      return client.get();
  }
  return nullptr;
}

std::vector<VideoCaptureController::BufferContext>::iterator
VideoCaptureController::FindBufferContextFromBufferContextId(
    int buffer_context_id) {
  return std::find_if(buffer_contexts_.begin(), buffer_contexts_.end(),
                      [buffer_context_id](const BufferContext& entry) {
                        return entry.buffer_context_id() == buffer_context_id;
                      });
}

std::vector<VideoCaptureController::BufferContext>::iterator
VideoCaptureController::FindUnretiredBufferContextFromBufferId(int buffer_id) {
  return std::find_if(buffer_contexts_.begin(), buffer_contexts_.end(),
                      [buffer_id](const BufferContext& entry) {
                        return (entry.buffer_id() == buffer_id) &&
                               (entry.is_retired() == false);
                      });
}

void VideoCaptureController::OnClientFinishedConsumingBuffer(
    ControllerClient* client,
    int buffer_context_id,
    double consumer_resource_utilization) {
  auto buffer_context_iter =
      FindBufferContextFromBufferContextId(buffer_context_id);
  DCHECK(buffer_context_iter != buffer_contexts_.end());

  buffer_context_iter->RecordConsumerUtilization(consumer_resource_utilization);
  buffer_context_iter->DecreaseConsumerCount();
  if (!buffer_context_iter->HasConsumers() &&
      buffer_context_iter->is_retired()) {
    ReleaseBufferContext(buffer_context_iter);
  }
}

void VideoCaptureController::ReleaseBufferContext(
    const std::vector<BufferContext>::iterator& buffer_context_iter) {
  for (const auto& client : controller_clients_) {
    if (client->session_closed)
      continue;
    auto entry_iter = std::find(std::begin(client->known_buffer_context_ids),
                                std::end(client->known_buffer_context_ids),
                                buffer_context_iter->buffer_context_id());
    if (entry_iter != std::end(client->known_buffer_context_ids)) {
      client->known_buffer_context_ids.erase(entry_iter);
      client->event_handler->OnBufferDestroyed(
          client->controller_id, buffer_context_iter->buffer_context_id());
    }
  }
  buffer_contexts_.erase(buffer_context_iter);
}

void VideoCaptureController::PerformForClientsWithOpenSession(
    EventHandlerAction action) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  for (const auto& client : controller_clients_) {
    if (client->session_closed)
      continue;
    action.Run(client->event_handler, client->controller_id);
  }
}

void VideoCaptureController::EmitLogMessage(const std::string& message,
                                            int verbose_log_level) {
  DVLOG(verbose_log_level) << message;
  emit_log_message_cb_.Run(message);
}

}  // namespace content

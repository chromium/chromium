// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/video_capture_controller.h"

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <set>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "base/token.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "media/capture/video/video_capture_buffer_pool.h"
#include "media/capture/video/video_capture_buffer_tracker_factory_impl.h"
#include "media/capture/video/video_capture_device_client.h"
#include "media/capture/video/video_capture_metrics.h"
#include "services/video_effects/public/mojom/video_effects_processor.mojom-forward.h"

#if !BUILDFLAG(IS_ANDROID)
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
  const raw_ptr<VideoCaptureControllerEventHandler> event_handler;

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
      consumer_hold_count_(0) {}

VideoCaptureController::BufferContext::~BufferContext() = default;

VideoCaptureController::BufferContext::BufferContext(
    VideoCaptureController::BufferContext&& other) = default;

VideoCaptureController::BufferContext& VideoCaptureController::BufferContext::
operator=(BufferContext&& other) = default;

void VideoCaptureController::BufferContext::RecordConsumerUtilization(
    const media::VideoCaptureFeedback& feedback) {
  combined_consumer_feedback_.Combine(feedback);
}

void VideoCaptureController::BufferContext::IncreaseConsumerCount() {
  consumer_hold_count_++;
}

void VideoCaptureController::BufferContext::DecreaseConsumerCount() {
  consumer_hold_count_--;
  if (consumer_hold_count_ == 0) {
    if (consumer_feedback_observer_ != nullptr &&
        !combined_consumer_feedback_.Empty()) {
      // We set this now since frame_feedback_id_ may be updated at anytime.
      combined_consumer_feedback_.frame_id = frame_feedback_id_;
      consumer_feedback_observer_->OnUtilizationReport(
          combined_consumer_feedback_);
    }
    buffer_read_permission_.reset();
    combined_consumer_feedback_ = media::VideoCaptureFeedback();
  }
}

media::mojom::VideoBufferHandlePtr
VideoCaptureController::BufferContext::CloneBufferHandle() {
  if (buffer_handle_->is_unsafe_shmem_region()) {
    // Buffer handles are always writable as they come from
    // VideoCaptureBufferPool which, among other use cases, provides decoder
    // output buffers.
    //
    // TODO(crbug.com/40553989): BroadcastingReceiver::BufferContext also
    // defines CloneBufferHandle and independently decides on handle
    // permissions. The permissions should be coordinated between these two
    // classes.
    return media::mojom::VideoBufferHandle::NewUnsafeShmemRegion(
        buffer_handle_->get_unsafe_shmem_region().Duplicate());
  } else if (buffer_handle_->is_read_only_shmem_region()) {
    return media::mojom::VideoBufferHandle::NewReadOnlyShmemRegion(
        buffer_handle_->get_read_only_shmem_region().Duplicate());
  } else if (buffer_handle_->is_shared_image_handle()) {
    return media::mojom::VideoBufferHandle::NewSharedImageHandle(
        buffer_handle_->get_shared_image_handle()->Clone());
  } else if (buffer_handle_->is_gpu_memory_buffer_handle()) {
    return media::mojom::VideoBufferHandle::NewGpuMemoryBufferHandle(
        buffer_handle_->get_gpu_memory_buffer_handle().Clone());
  } else {
    NOTREACHED_IN_MIGRATION() << "Unexpected video buffer handle type";
    return media::mojom::VideoBufferHandlePtr();
  }
}

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
    const media::VideoCaptureParams& params,
    std::optional<url::Origin> origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  std::ostringstream string_stream;
  string_stream << "VideoCaptureController::AddClient(): id = " << id
                << ", session_id = " << session_id.ToString()
                << ", params.requested_format = "
                << media::VideoCaptureFormat::ToString(params.requested_format);
  EmitLogMessage(string_stream.str(), 1);

  // Params received from a renderer will have been validated by
  // VideoCaptureHost, so here we can just require validity.
  DCHECK(params.IsValid());

  // Check that requested VideoCaptureParams are supported.  If not, report an
  // error immediately and punt.
  if (!(params.requested_format.pixel_format == media::PIXEL_FORMAT_I420 ||
        params.requested_format.pixel_format == media::PIXEL_FORMAT_Y16 ||
        params.requested_format.pixel_format == media::PIXEL_FORMAT_ARGB ||
        params.requested_format.pixel_format == media::PIXEL_FORMAT_NV12 ||
        params.requested_format.pixel_format == media::PIXEL_FORMAT_UNKNOWN)) {
    // Crash in debug builds since the renderer should not have asked for
    // unsupported parameters.
    LOG(DFATAL) << "Unsupported video capture parameters requested: "
                << media::VideoCaptureFormat::ToString(params.requested_format);
    event_handler->OnError(id,
                           media::VideoCaptureError::
                               kVideoCaptureControllerUnsupportedPixelFormat);
    return;
  }

  // If this is the first client added to the controller, cache the parameters.
  if (controller_clients_.empty()) {
    video_capture_format_ = params.requested_format;
    first_client_origin_ = origin;
  }

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
    OnClientFinishedConsumingBuffer(client, buffer_id,
                                    media::VideoCaptureFeedback());
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
    const media::VideoCaptureFeedback& feedback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  ControllerClient* client = FindClient(id, event_handler, controller_clients_);
  CHECK(client);

  auto buffers_in_use_entry_iter =
      base::ranges::find(client->buffers_in_use, buffer_id);
  CHECK(buffers_in_use_entry_iter != std::end(client->buffers_in_use));
  client->buffers_in_use.erase(buffers_in_use_entry_iter);

  OnClientFinishedConsumingBuffer(client, buffer_id, feedback);
}

const std::optional<media::VideoCaptureFormat>
VideoCaptureController::GetVideoCaptureFormat() const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return video_capture_format_;
}

const std::optional<url::Origin> VideoCaptureController::GetFirstClientOrigin()
    const {
  return first_client_origin_;
}

void VideoCaptureController::OnCaptureConfigurationChanged() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  EmitLogMessage(__func__, 3);
  for (const auto& client : controller_clients_) {
    if (client->session_closed) {
      continue;
    }
    client->event_handler->OnCaptureConfigurationChanged(client->controller_id);
  }
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
    media::ReadyFrameInBuffer frame) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_NE(frame.buffer_id, media::VideoCaptureBufferPool::kInvalidId);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "VideoCaptureController::OnFrameReadyInBuffer");

  // Make ready buffers, get frame contexts and set their feedback IDs.
  // Transfer ownership of all the frame infos.
  BufferContext* frame_context;
  ReadyBuffer frame_ready_buffer = MakeReadyBufferAndSetContextFeedbackId(
      frame.buffer_id, frame.frame_feedback_id, std::move(frame.frame_info),
      &frame_context);

  if (state_ != blink::VIDEO_CAPTURE_STATE_ERROR) {
    // Inform all active clients of the frames.
    for (const auto& client : controller_clients_) {
      if (client->session_closed || client->paused)
        continue;
      MakeClientUseBufferContext(frame_context, client.get());
      client->event_handler->OnBufferReady(client->controller_id,
                                           frame_ready_buffer);
    }
    // Transfer buffer read permissions to any contexts that now have consumers.
    if (frame_context->HasConsumers()) {
      frame_context->set_read_permission(
          std::move(frame.buffer_read_permission));
    }
  }

  if (!has_received_frames_) {
    // Check the following group of metrics is captured only for cameras.
    if (stream_type() == blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE) {
      // This metric combines width and height into a single UMA metric.
      media::LogCaptureCurrentDeviceResolution(
          frame_ready_buffer.frame_info->coded_size.width(),
          frame_ready_buffer.frame_info->coded_size.height());

      media::LogCaptureCurrentDevicePixelFormat(
          frame_ready_buffer.frame_info->pixel_format);
    }
    UMA_HISTOGRAM_COUNTS_1M("Media.VideoCapture.Width",
                            frame_ready_buffer.frame_info->coded_size.width());
    UMA_HISTOGRAM_COUNTS_1M("Media.VideoCapture.Height",
                            frame_ready_buffer.frame_info->coded_size.height());
    UMA_HISTOGRAM_ASPECT_RATIO(
        "Media.VideoCapture.AspectRatio",
        frame_ready_buffer.frame_info->coded_size.width(),
        frame_ready_buffer.frame_info->coded_size.height());
    double frame_rate = 0.0f;
    if (video_capture_format_) {
      frame_rate = frame_ready_buffer.frame_info->metadata.frame_rate.value_or(
          video_capture_format_->frame_rate);
    }
    UMA_HISTOGRAM_COUNTS_1M("Media.VideoCapture.FrameRate", frame_rate);
    UMA_HISTOGRAM_TIMES("Media.VideoCapture.DelayUntilFirstFrame",
                        base::TimeTicks::Now() - time_of_start_request_);
    OnLog("First frame received at VideoCaptureController");
    has_received_frames_ = true;
  }
}

ReadyBuffer VideoCaptureController::MakeReadyBufferAndSetContextFeedbackId(
    int buffer_id,
    int frame_feedback_id,
    media::mojom::VideoFrameInfoPtr frame_info,
    BufferContext** out_buffer_context) {
  auto buffer_context_iter = FindUnretiredBufferContextFromBufferId(buffer_id);
  CHECK(buffer_context_iter != buffer_contexts_.end(),
        base::NotFatalUntil::M130);
  BufferContext* buffer_context = &(*buffer_context_iter);
  buffer_context->set_frame_feedback_id(frame_feedback_id);
  DCHECK(!buffer_context->HasConsumers());
  *out_buffer_context = buffer_context;
  return ReadyBuffer(buffer_context->buffer_context_id(),
                     std::move(frame_info));
}

void VideoCaptureController::MakeClientUseBufferContext(
    BufferContext* frame_context,
    ControllerClient* client) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // On the first use of a BufferContext for a particular client, call
  // OnBufferCreated().
  if (!base::Contains(client->known_buffer_context_ids,
                      frame_context->buffer_context_id())) {
    client->known_buffer_context_ids.push_back(
        frame_context->buffer_context_id());
    client->event_handler->OnNewBuffer(client->controller_id,
                                       frame_context->CloneBufferHandle(),
                                       frame_context->buffer_context_id());
  }
  // Ensure buffer is registered as in use by the client.
  if (!base::Contains(client->buffers_in_use,
                      frame_context->buffer_context_id())) {
    client->buffers_in_use.push_back(frame_context->buffer_context_id());
  } else {
    NOTREACHED_IN_MIGRATION() << "Unexpected duplicate buffer: "
                              << frame_context->buffer_context_id();
  }
  frame_context->IncreaseConsumerCount();
}

void VideoCaptureController::OnBufferRetired(int buffer_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto buffer_context_iter = FindUnretiredBufferContextFromBufferId(buffer_id);
  CHECK(buffer_context_iter != buffer_contexts_.end(),
        base::NotFatalUntil::M130);

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
  // This method implements media::VideoFrameReceiver, which implements signals
  // between the capture process and browser process. We forward this call to
  // the renderer process where it eventually reached the MediaStreamVideoTrack.
  for (const auto& client : controller_clients_) {
    if (client->session_closed) {
      continue;
    }
    client->event_handler->OnFrameDropped(client->controller_id, reason);
  }
}

void VideoCaptureController::OnNewSubCaptureTargetVersion(
    uint32_t sub_capture_target_version) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  EmitLogMessage(
      base::StringPrintf("%s(%u)", __func__, sub_capture_target_version), 3);
  for (const auto& client : controller_clients_) {
    if (client->session_closed) {
      continue;
    }
    client->event_handler->OnNewSubCaptureTargetVersion(
        client->controller_id, sub_capture_target_version);
  }
}

void VideoCaptureController::OnFrameWithEmptyRegionCapture() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  EmitLogMessage(__func__, 3);
  for (const auto& client : controller_clients_) {
    if (client->session_closed) {
      continue;
    }
    client->event_handler->OnFrameWithEmptyRegionCapture(client->controller_id);
  }
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
    base::OnceClosure done_cb,
    mojo::PendingRemote<video_effects::mojom::VideoEffectsProcessor>
        video_effects_processor) {
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
      this, std::move(done_cb), std::move(video_effects_processor));
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
  // |buffer_contexts_| contain references to |launched_device_| as observers.
  // Clear those observer references prior to resetting |launced_device_|.
  for (auto& entry : buffer_contexts_)
    entry.set_consumer_feedback_observer(nullptr);
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

void VideoCaptureController::ApplySubCaptureTarget(
    media::mojom::SubCaptureTargetType type,
    const base::Token& target,
    uint32_t sub_capture_target_version,
    base::OnceCallback<void(media::mojom::ApplySubCaptureTargetResult)>
        callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(launched_device_);

  EmitLogMessage(__func__, 3);

  was_crop_ever_called_ = true;

  if (controller_clients_.size() != 1) {
    std::move(callback).Run(
        media::mojom::ApplySubCaptureTargetResult::kNotImplemented);
    return;
  }

  launched_device_->ApplySubCaptureTarget(
      type, target, sub_capture_target_version, std::move(callback));
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
  return base::ranges::find(buffer_contexts_, buffer_context_id,
                            &BufferContext::buffer_context_id);
}

std::vector<VideoCaptureController::BufferContext>::iterator
VideoCaptureController::FindUnretiredBufferContextFromBufferId(int buffer_id) {
  return base::ranges::find_if(
      buffer_contexts_, [buffer_id](const BufferContext& entry) {
        return (entry.buffer_id() == buffer_id) && !entry.is_retired();
      });
}

void VideoCaptureController::OnClientFinishedConsumingBuffer(
    ControllerClient* client,
    int buffer_context_id,
    const media::VideoCaptureFeedback& feedback) {
  auto buffer_context_iter =
      FindBufferContextFromBufferContextId(buffer_context_id);
  CHECK(buffer_context_iter != buffer_contexts_.end(),
        base::NotFatalUntil::M130);

  buffer_context_iter->RecordConsumerUtilization(feedback);
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
    auto entry_iter =
        base::ranges::find(client->known_buffer_context_ids,
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
    action.Run(client->event_handler.get(), client->controller_id);
  }
}

void VideoCaptureController::EmitLogMessage(const std::string& message,
                                            int verbose_log_level) {
  DVLOG(verbose_log_level) << message;
  emit_log_message_cb_.Run(message);
}

}  // namespace content

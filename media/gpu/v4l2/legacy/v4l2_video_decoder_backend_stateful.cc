// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/legacy/v4l2_video_decoder_backend_stateful.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <tuple>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/limits.h"
#include "media/base/platform_features.h"
#include "media/base/video_codecs.h"
#include "media/gpu/chromeos/dmabuf_video_frame_pool.h"
#include "media/gpu/chromeos/platform_video_frame_utils.h"
#include "media/gpu/chromeos/video_frame_resource.h"
#include "media/gpu/macros.h"
#include "media/gpu/v4l2/v4l2_device.h"
#include "media/gpu/v4l2/v4l2_vda_helpers.h"
#include "media/gpu/v4l2/v4l2_video_decoder_backend.h"
#include "media/gpu/v4l2/v4l2_vp9_helpers.h"

namespace media {

namespace {

bool IsVp9KSVCStream(VideoCodecProfile profile,
                     const DecoderBuffer& decoder_buffer) {
  return VideoCodecProfileToVideoCodec(profile) == VideoCodec::kVP9 &&
         decoder_buffer.has_side_data() &&
         !decoder_buffer.side_data()->spatial_layers.empty();
}

bool IsVp9KSVCSupportedDriver(const std::string& driver_name) {
  const std::string kVP9KSVCSupportedDrivers[] = {"qcom-venus"};
  return base::Contains(kVP9KSVCSupportedDrivers, driver_name);
}

std::optional<uint8_t> V4L2PixelFormatToBitDepth(uint32_t v4l2_pixelformat) {
  const auto fourcc = Fourcc::FromV4L2PixFmt(v4l2_pixelformat);
  if (fourcc) {
    return BitDepth(fourcc->ToVideoPixelFormat());
  }

  return std::nullopt;
}
}  // namespace

V4L2StatefulVideoDecoderBackend::DecodeRequest::DecodeRequest(
    scoped_refptr<DecoderBuffer> buf,
    VideoDecoder::DecodeCB cb)
    : buffer(std::move(buf)), decode_cb(std::move(cb)) {}

V4L2StatefulVideoDecoderBackend::DecodeRequest::DecodeRequest(DecodeRequest&&) =
    default;
V4L2StatefulVideoDecoderBackend::DecodeRequest&
V4L2StatefulVideoDecoderBackend::DecodeRequest::operator=(DecodeRequest&&) =
    default;

V4L2StatefulVideoDecoderBackend::DecodeRequest::~DecodeRequest() = default;

bool V4L2StatefulVideoDecoderBackend::DecodeRequest::IsCompleted() const {
  return bytes_used == buffer->size();
}

V4L2StatefulVideoDecoderBackend::V4L2StatefulVideoDecoderBackend(
    Client* const client,
    scoped_refptr<V4L2Device> device,
    VideoCodecProfile profile,
    const VideoColorSpace& color_space,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : V4L2VideoDecoderBackend(client, std::move(device)),
      driver_name_(device_->GetDriverName()),
      profile_(profile),
      color_space_(color_space),
      task_runner_(task_runner) {
  DVLOGF(3);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weak_this_ = weak_this_factory_.GetWeakPtr();
}

V4L2StatefulVideoDecoderBackend::~V4L2StatefulVideoDecoderBackend() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(3);

  if (flush_cb_ || current_decode_request_ || !decode_request_queue_.empty()) {
    VLOGF(1) << "Should not destroy backend during pending decode!";
  }

  struct v4l2_event_subscription sub;
  memset(&sub, 0, sizeof(sub));
  sub.type = V4L2_EVENT_SOURCE_CHANGE;
  if (device_->Ioctl(VIDIOC_UNSUBSCRIBE_EVENT, &sub) != 0) {
    VLOGF(1) << "Cannot unsubscribe to event";
  }
}

bool V4L2StatefulVideoDecoderBackend::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(3);

  if (!IsSupportedProfile(profile_)) {
    VLOGF(1) << "Unsupported profile " << GetProfileName(profile_);
    return false;
  }

  frame_splitter_ =
      v4l2_vda_helpers::InputBufferFragmentSplitter::CreateFromProfile(
          profile_);
  if (!frame_splitter_) {
    VLOGF(1) << "Failed to create frame splitter";
    return false;
  }

  struct v4l2_event_subscription sub;
  memset(&sub, 0, sizeof(sub));
  sub.type = V4L2_EVENT_SOURCE_CHANGE;
  if (device_->Ioctl(VIDIOC_SUBSCRIBE_EVENT, &sub) != 0) {
    VLOGF(1) << "Cannot subscribe to event";
    return false;
  }

  framerate_control_ = std::make_unique<V4L2FrameRateControl>(
      base::BindRepeating(&V4L2Device::Ioctl, device_), task_runner_);

  return true;
}

void V4L2StatefulVideoDecoderBackend::EnqueueDecodeTask(
    scoped_refptr<DecoderBuffer> buffer,
    VideoDecoder::DecodeCB decode_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(3);

  if (!buffer->end_of_stream()) {
    has_pending_requests_ = true;
  }

  decode_request_queue_.push(
      DecodeRequest(std::move(buffer), std::move(decode_cb)));

  DoDecodeWork();
}

void V4L2StatefulVideoDecoderBackend::DoDecodeWork() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(3);

  // Do not decode if a flush or resolution change is in progress.
  if (!client_->IsDecoding())
    return;

  if (need_resume_resolution_change_) {
    need_resume_resolution_change_ = false;
    ChangeResolution();
    if (!client_->IsDecoding())
      return;
  }

  // Get a new decode request if none is in progress.
  if (!current_decode_request_) {
    // No more decode request, nothing to do for now.
    if (decode_request_queue_.empty())
      return;

    auto decode_request = std::move(decode_request_queue_.front());
    decode_request_queue_.pop();

    // Need to flush?
    if (decode_request.buffer->end_of_stream()) {
      InitiateFlush(std::move(decode_request.decode_cb));
      return;
    }

    // This is our new decode request.
    current_decode_request_ = std::move(decode_request);
    DCHECK_EQ(current_decode_request_->bytes_used, 0u);

    if (IsVp9KSVCStream(profile_, *current_decode_request_->buffer)) {
      if (!IsVp9KSVCSupportedDriver(driver_name_)) {
        DLOG(ERROR) << driver_name_ << " doesn't support VP9 k-SVC decoding";
        client_->OnBackendError();
        return;
      }

      if (!AppendVP9SuperFrameIndex(current_decode_request_->buffer)) {
        LOG(ERROR) << "Failed to append superframe index for VP9 k-SVC frame";
        client_->OnBackendError();
        return;
      }
    }
  }

  // Get a V4L2 buffer to copy the encoded data into.
  if (!current_input_buffer_) {
    current_input_buffer_ = input_queue_->GetFreeBuffer();
    // We will be called again once an input buffer becomes available.
    if (!current_input_buffer_)
      return;

    // Record timestamp of the input buffer so it propagates to the decoded
    // frames.
    // TODO(mcasas): Consider using TimeDeltaToTimeVal().
    const struct timespec timespec =
        current_decode_request_->buffer->timestamp().ToTimeSpec();
    struct timeval timestamp = {
        .tv_sec = timespec.tv_sec,
        .tv_usec = timespec.tv_nsec / 1000,
    };
    current_input_buffer_->SetTimeStamp(timestamp);

    const int64_t flat_timespec =
        base::TimeDelta::FromTimeSpec(timespec).InMilliseconds();
    encoding_timestamps_[flat_timespec] = base::TimeTicks::Now();
  }

  // From here on we have both a decode request and input buffer, so we can
  // progress with decoding.
  DCHECK(current_decode_request_.has_value());
  DCHECK(current_input_buffer_.has_value());

  const DecoderBuffer* current_buffer = current_decode_request_->buffer.get();
  DCHECK_LT(current_decode_request_->bytes_used, current_buffer->size());
  const uint8_t* const data =
      current_buffer->data() + current_decode_request_->bytes_used;
  const size_t data_size =
      current_buffer->size() - current_decode_request_->bytes_used;
  size_t bytes_to_copy = 0;

  if (!frame_splitter_->AdvanceFrameFragment(data, data_size, &bytes_to_copy)) {
    LOG(ERROR) << "Invalid bitstream detected.";
    std::move(current_decode_request_->decode_cb)
        .Run(DecoderStatus::Codes::kFailed);
    current_decode_request_.reset();
    current_input_buffer_.reset();
    client_->OnBackendError();
    return;
  }

  const size_t bytes_used = current_input_buffer_->GetPlaneBytesUsed(0);
  if (bytes_used + bytes_to_copy > current_input_buffer_->GetPlaneSize(0)) {
    LOG(ERROR) << "V4L2 buffer size is too small to contain a whole frame.";
    std::move(current_decode_request_->decode_cb)
        .Run(DecoderStatus::Codes::kFailed);
    current_decode_request_.reset();
    current_input_buffer_.reset();
    client_->OnBackendError();
    return;
  }

  uint8_t* dst =
      static_cast<uint8_t*>(current_input_buffer_->GetPlaneMapping(0)) +
      bytes_used;
  memcpy(dst, data, bytes_to_copy);
  current_input_buffer_->SetPlaneBytesUsed(0, bytes_used + bytes_to_copy);
  current_decode_request_->bytes_used += bytes_to_copy;

  // Release current_input_request_ if we reached its end.
  if (current_decode_request_->IsCompleted()) {
    std::move(current_decode_request_->decode_cb)
        .Run(DecoderStatus::Codes::kOk);
    current_decode_request_.reset();
  }

  // If we have a partial frame, wait before submitting it.
  if (frame_splitter_->IsPartialFramePending()) {
    VLOGF(4) << "Partial frame pending, not queueing any buffer now.";
    return;
  }

  // The V4L2 input buffer contains a decodable entity, queue it.
  if (!std::move(*current_input_buffer_).QueueMMap()) {
    LOG(ERROR) << "Error while queuing input buffer!";
    client_->OnBackendError();
  }
  current_input_buffer_.reset();

  // If we can still progress on a decode request, do it.
  if (current_decode_request_ || !decode_request_queue_.empty())
    ScheduleDecodeWork();
}

void V4L2StatefulVideoDecoderBackend::ScheduleDecodeWork() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(3);

  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&V4L2StatefulVideoDecoderBackend::DoDecodeWork,
                                weak_this_));
}

void V4L2StatefulVideoDecoderBackend::ProcessEventQueue() {
  while (std::optional<struct v4l2_event> ev = device_->DequeueEvent()) {
    if (ev->type == V4L2_EVENT_SOURCE_CHANGE &&
        (ev->u.src_change.changes & V4L2_EVENT_SRC_CH_RESOLUTION)) {
      ChangeResolution();
    }
  }
}

void V4L2StatefulVideoDecoderBackend::OnServiceDeviceTask(bool event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(3);

  if (event)
    ProcessEventQueue();

  // We can enqueue dequeued output buffers immediately.
  EnqueueOutputBuffers();

  // Try to progress on our work since we may have dequeued input buffers.
  DoDecodeWork();
}

void V4L2StatefulVideoDecoderBackend::EnqueueOutputBuffers() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(3);
  const v4l2_memory mem_type = output_queue_->GetMemoryType();

  while (true) {
    bool ret = false;
    bool no_buffer = false;

    std::optional<V4L2WritableBufferRef> buffer;
    switch (mem_type) {
      case V4L2_MEMORY_MMAP:
        buffer = output_queue_->GetFreeBuffer();
        if (!buffer) {
          no_buffer = true;
          break;
        }

        ret = std::move(*buffer).QueueMMap();
        break;
      case V4L2_MEMORY_DMABUF: {
        scoped_refptr<FrameResource> frame = GetPoolVideoFrame();
        // Running out of frame is not an error, we will be called again
        // once frames are available.
        if (!frame) {
          return;
        }
        buffer =
            output_queue_->GetFreeBufferForFrame(frame->GetSharedMemoryId());
        if (!buffer) {
          no_buffer = true;
          break;
        }

        framerate_control_->AttachToFrameResource(frame);
        ret = std::move(*buffer).QueueDMABuf(std::move(frame));
        break;
      }
      default:
        NOTREACHED_IN_MIGRATION();
    }

    // Running out of V4L2 buffers is not an error, so just exit the loop
    // gracefully.
    if (no_buffer)
      break;

    if (!ret) {
      LOG(ERROR) << "Error while queueing output buffer!";
      client_->OnBackendError();
    }
  }

  DVLOGF(3) << output_queue_->QueuedBuffersCount() << "/"
            << output_queue_->AllocatedBuffersCount()
            << " output buffers queued";
}

scoped_refptr<FrameResource>
V4L2StatefulVideoDecoderBackend::GetPoolVideoFrame() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(3);
  DmabufVideoFramePool* pool = client_->GetVideoFramePool();
  DCHECK_EQ(output_queue_->GetMemoryType(), V4L2_MEMORY_DMABUF);
  DCHECK_NE(pool, nullptr);

  scoped_refptr<FrameResource> frame = pool->GetFrame();
  if (!frame) {
    DVLOGF(3) << "No available VideoFrame for now";
    // We will try again once a frame becomes available.
    pool->NotifyWhenFrameAvailable(base::BindOnce(
        base::IgnoreResult(&base::SequencedTaskRunner::PostTask), task_runner_,
        FROM_HERE,
        base::BindOnce(
            base::IgnoreResult(
                &V4L2StatefulVideoDecoderBackend::EnqueueOutputBuffers),
            weak_this_)));
  }
  return frame;
}

// static
void V4L2StatefulVideoDecoderBackend::ReuseOutputBufferThunk(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    std::optional<base::WeakPtr<V4L2StatefulVideoDecoderBackend>> weak_this,
    V4L2ReadableBufferRef buffer) {
  DVLOGF(3);
  DCHECK(weak_this);

  if (task_runner->RunsTasksInCurrentSequence()) {
    if (*weak_this)
      (*weak_this)->ReuseOutputBuffer(std::move(buffer));
  } else {
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&V4L2StatefulVideoDecoderBackend::ReuseOutputBuffer,
                       *weak_this, std::move(buffer)));
  }
}

void V4L2StatefulVideoDecoderBackend::ReuseOutputBuffer(
    V4L2ReadableBufferRef buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(3) << "Reuse output buffer #" << buffer->BufferId();

  // Lose reference to the buffer so it goes back to the free list.
  buffer.reset();

  // Enqueue the newly available buffer.
  EnqueueOutputBuffers();
}

void V4L2StatefulVideoDecoderBackend::OnOutputBufferDequeued(
    V4L2ReadableBufferRef buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(3);

  // Zero-bytes buffers are returned as part of a flush and can be dismissed.
  if (buffer->GetPlaneBytesUsed(0) > 0) {
    // TODO(mcasas): Consider using TimeValToTimeDelta().
    const struct timeval timeval = buffer->GetTimeStamp();
    const struct timespec timespec = {
        .tv_sec = timeval.tv_sec,
        .tv_nsec = timeval.tv_usec * 1000,
    };

    const int64_t flat_timespec =
        base::TimeDelta::FromTimeSpec(timespec).InMilliseconds();
    // TODO(b/190615065) |flat_timespec| might be repeated with H.264
    // bitstreams, investigate why, and change the if() to DCHECK().
    if (base::Contains(encoding_timestamps_, flat_timespec)) {
      UMA_HISTOGRAM_TIMES(
          "Media.PlatformVideoDecoding.Decode",
          base::TimeTicks::Now() - encoding_timestamps_[flat_timespec]);
      encoding_timestamps_.erase(flat_timespec);
    }

    scoped_refptr<FrameResource> frame;

    switch (output_queue_->GetMemoryType()) {
      case V4L2_MEMORY_MMAP: {
        // Wrap the frame into another one so we can be signaled when the
        // consumer is done with it and reuse the V4L2 buffer.
        scoped_refptr<FrameResource> origin_frame = buffer->GetFrameResource();
        frame = origin_frame->CreateWrappingFrame();
        frame->AddDestructionObserver(base::BindOnce(
            &V4L2StatefulVideoDecoderBackend::ReuseOutputBufferThunk,
            task_runner_, weak_this_, buffer));
        break;
      }
      case V4L2_MEMORY_DMABUF:
        // The frame from the frame pool that we passed to QueueDMABuf() has
        // been decoded into. It can be output as-is.
        frame = buffer->GetFrameResource();
        break;
      default:
        NOTREACHED_IN_MIGRATION();
    }

    const base::TimeDelta timestamp = base::TimeDelta::FromTimeSpec(timespec);
    // TODO(b/214190092): Get color space from the buffer.
    client_->OutputFrame(std::move(frame), *visible_rect_, color_space_,
                         timestamp);
  }

  // We were waiting for the last buffer before a resolution change
  // The order here is important! A flush event may come after a resolution
  // change event (but not the opposite), so we must make sure both events
  // are processed in the correct order.
  if (buffer->IsLast()){
    // Check that we don't have a resolution change event pending. If we do
    // then this LAST buffer was related to it.
    ProcessEventQueue();

    if (resolution_change_cb_) {
      std::move(resolution_change_cb_).Run();
    } else if (flush_cb_) {
      // We were waiting for a flush to complete, and received the last buffer.
      CompleteFlush();
    }
  }

  EnqueueOutputBuffers();
}

bool V4L2StatefulVideoDecoderBackend::InitiateFlush(
    VideoDecoder::DecodeCB flush_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(3);
  DCHECK(!flush_cb_);

  // Submit any pending input buffer at the time of flush.
  if (current_input_buffer_) {
    if (!std::move(*current_input_buffer_).QueueMMap()) {
      LOG(ERROR) << "Error while queuing input buffer!";
      client_->OnBackendError();
    }
    current_input_buffer_.reset();
  }

  client_->InitiateFlush();
  flush_cb_ = std::move(flush_cb);

  // The stream could be stopped in the middle of the frame when the flush is
  // being triggered. This makes sure there are no leftovers after the flush
  // finishes.
  frame_splitter_->Reset();

  // Special case: if we haven't received any decoding request, we could
  // complete the flush immediately.
  if (!has_pending_requests_)
    return CompleteFlush();

  if (output_queue_->IsStreaming()) {
    // If the CAPTURE queue is streaming, send the STOP command to the V4L2
    // device. The device will let us know that the flush is completed by
    // sending us a CAPTURE buffer with the LAST flag set.
    return output_queue_->SendStopCommand();
  } else {
    // If the CAPTURE queue is not streaming, this means we received the flush
    // request before the initial resolution has been established. The flush
    // request will be processed in OnChangeResolutionDone(), when the CAPTURE
    // queue starts streaming.
    DVLOGF(2) << "Flush request to be processed after CAPTURE queue starts";
  }

  return true;
}

bool V4L2StatefulVideoDecoderBackend::CompleteFlush() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(3);
  DCHECK(flush_cb_);

  // Signal that flush has properly been completed.
  std::move(flush_cb_).Run(DecoderStatus::Codes::kOk);

  // If CAPTURE queue is streaming, send the START command to the V4L2 device
  // to signal that we are resuming decoding with the same state.
  if (output_queue_->IsStreaming() && !output_queue_->SendStartCommand()) {
    LOG(ERROR) << "Failed to issue START command";
    std::move(flush_cb_).Run(DecoderStatus::Codes::kFailed);
    client_->OnBackendError();
    return false;
  }

  client_->CompleteFlush();
  // Qualcomm venus stops capture queue after LAST buffer is dequeued and needs
  // restarting to be ready for resume operation in case it was left in EOS
  // state
  client_->RestartStream();
  // Resume decoding if data is available.
  ScheduleDecodeWork();

  has_pending_requests_ = false;
  return true;
}

void V4L2StatefulVideoDecoderBackend::OnStreamStopped(bool stop_input_queue) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(3);

  // If we are resetting, also reset the splitter.
  if (frame_splitter_ && stop_input_queue)
    frame_splitter_->Reset();
}

void V4L2StatefulVideoDecoderBackend::ChangeResolution() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(3);

  // Here we just query the new resolution, visible rect, and number of output
  // buffers before asking the client to update the resolution.

  auto format = output_queue_->GetFormat().first;
  if (!format) {
    LOG(ERROR) << "Unable to get format when changing resolution.";
    client_->OnBackendError();
    return;
  }
  const gfx::Size pic_size(format->fmt.pix_mp.width, format->fmt.pix_mp.height);

  auto visible_rect = output_queue_->GetVisibleRect();
  if (!visible_rect) {
    LOG(ERROR) << "Unable to get visible rectangle when changing resolution.";
    client_->OnBackendError();
    return;
  }

  if (!gfx::Rect(pic_size).Contains(*visible_rect)) {
    LOG(ERROR) << "Visible rectangle (" << visible_rect->ToString()
               << ") is not contained by the picture rectangle ("
               << gfx::Rect(pic_size).ToString() << ").";
    client_->OnBackendError();
    return;
  }

  const auto bit_depth =
      V4L2PixelFormatToBitDepth(format->fmt.pix_mp.pixelformat);
  if (!bit_depth) {
    LOG(ERROR) << "Unable to determine bitdepth of format ";
    client_->OnBackendError();
    return;
  }

  // Estimate the amount of buffers needed for the CAPTURE queue and for codec
  // reference requirements. For VP9 and AV1, the maximum number of reference
  // frames is constant and 8 (for VP8 is 4); for H.264 and other ITU-T codecs,
  // it depends on the bitstream. Here we query it from the driver anyway.
  constexpr size_t kDefaultNumReferenceFrames = 8;
  size_t num_codec_reference_frames = kDefaultNumReferenceFrames;
  // On QC Venus, this control ranges between 1 and 32 at the time of writing.
  auto ctrl = device_->GetCtrl(V4L2_CID_MIN_BUFFERS_FOR_CAPTURE);
  if (ctrl) {
    VLOGF(2) << "V4L2_CID_MIN_BUFFERS_FOR_CAPTURE  = " << ctrl->value;
    num_codec_reference_frames = std::max(
        base::checked_cast<size_t>(ctrl->value), num_codec_reference_frames);
  }
  // Verify |num_codec_reference_frames| has a reasonable value. Anecdotally 16
  // is the largest amount of reference frames seen, on an ITU-T H.264 test
  // vector (CAPCM*1_Sand_E.h264).
  CHECK_LE(num_codec_reference_frames, 32u);

  // Signal that we are flushing and initiate the resolution change.
  // Our flush will be done when we receive a buffer with the LAST flag on the
  // CAPTURE queue.
  client_->InitiateFlush();
  DCHECK(!resolution_change_cb_);
  resolution_change_cb_ = base::BindOnce(
      &V4L2StatefulVideoDecoderBackend::ContinueChangeResolution, weak_this_,
      pic_size, *visible_rect, num_codec_reference_frames, *bit_depth);

  // ...that is, unless we are not streaming yet, in which case the resolution
  // change can take place immediately.
  if (!output_queue_->IsStreaming())
    std::move(resolution_change_cb_).Run();
}

void V4L2StatefulVideoDecoderBackend::ContinueChangeResolution(
    const gfx::Size& pic_size,
    const gfx::Rect& visible_rect,
    const size_t num_codec_reference_frames,
    const uint8_t bit_depth) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(3);

  // Flush is done, but stay in flushing state and ask our client to set the new
  // resolution.
  client_->ChangeResolution(pic_size, visible_rect, num_codec_reference_frames,
                            bit_depth);
}

bool V4L2StatefulVideoDecoderBackend::ApplyResolution(
    const gfx::Size& pic_size,
    const gfx::Rect& visible_rect) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(3);

  // Use the visible rect for all new frames.
  visible_rect_ = visible_rect;

  return true;
}

void V4L2StatefulVideoDecoderBackend::OnChangeResolutionDone(CroStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(3) << "status=" << static_cast<int>(status.code());

  if (status == CroStatus::Codes::kResetRequired) {
    need_resume_resolution_change_ = true;
    return;
  }

  if (status != CroStatus::Codes::kOk) {
    LOG(ERROR) << "Backend failure when changing resolution ("
               << static_cast<int>(status.code()) << ").";
    client_->OnBackendError();
    return;
  }

  // Flush can be considered completed on the client side.
  client_->CompleteFlush();

  // Enqueue all available output buffers now that they are allocated.
  EnqueueOutputBuffers();

  // If we had a flush request pending before the initial resolution change,
  // process it now.
  if (flush_cb_) {
    DVLOGF(2) << "Processing pending flush request...";

    client_->InitiateFlush();
    if (!output_queue_->SendStopCommand()) {
      return;
    }
  }

  // Also try to progress on our work.
  DoDecodeWork();
}

void V4L2StatefulVideoDecoderBackend::ClearPendingRequests(
    DecoderStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(3);

  if (resolution_change_cb_)
    std::move(resolution_change_cb_).Run();

  if (flush_cb_) {
    std::move(flush_cb_).Run(status);
  }

  current_input_buffer_.reset();

  if (current_decode_request_) {
    std::move(current_decode_request_->decode_cb).Run(status);
    current_decode_request_.reset();
  }

  while (!decode_request_queue_.empty()) {
    std::move(decode_request_queue_.front().decode_cb).Run(status);
    decode_request_queue_.pop();
  }

  has_pending_requests_ = false;
}

// TODO(b:149663704) move into helper function shared between both backends?
bool V4L2StatefulVideoDecoderBackend::IsSupportedProfile(
    VideoCodecProfile profile) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(device_);
  if (supported_profiles_.empty()) {
    const std::vector<uint32_t> kSupportedInputFourccs = {
      V4L2_PIX_FMT_H264,
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
      V4L2_PIX_FMT_HEVC,
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
      V4L2_PIX_FMT_VP8,
      V4L2_PIX_FMT_VP9,
    };
    auto device = base::MakeRefCounted<V4L2Device>();
    VideoDecodeAccelerator::SupportedProfiles profiles =
        device->GetSupportedDecodeProfiles(kSupportedInputFourccs);
    for (const auto& entry : profiles)
      supported_profiles_.push_back(entry.profile);
  }
  return base::Contains(supported_profiles_, profile);
}

bool V4L2StatefulVideoDecoderBackend::StopInputQueueOnResChange() const {
  return false;
}

size_t V4L2StatefulVideoDecoderBackend::GetNumOUTPUTQueueBuffers(
    bool secure_mode) const {
  CHECK(!secure_mode);
  constexpr size_t kNumInputBuffers = 8;
  return kNumInputBuffers;
}

}  // namespace media

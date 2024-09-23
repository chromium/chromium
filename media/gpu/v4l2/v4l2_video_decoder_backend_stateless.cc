// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/v4l2_video_decoder_backend_stateless.h"

#include <fcntl.h>
#include <linux/media.h>
#include <sys/ioctl.h>

#include <memory>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/numerics/safe_conversions.h"
#include "base/posix/eintr_wrapper.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "media/base/decoder_status.h"
#include "media/base/video_codecs.h"
#include "media/base/video_frame.h"
#include "media/gpu/accelerated_video_decoder.h"
#include "media/gpu/chromeos/dmabuf_video_frame_pool.h"
#include "media/gpu/chromeos/video_frame_resource.h"
#include "media/gpu/macros.h"
#include "media/gpu/v4l2/v4l2_device.h"
#include "media/gpu/v4l2/v4l2_video_decoder_delegate_av1.h"
#include "media/gpu/v4l2/v4l2_video_decoder_delegate_h264.h"
#include "media/gpu/v4l2/v4l2_video_decoder_delegate_h265.h"
#include "media/gpu/v4l2/v4l2_video_decoder_delegate_vp8.h"
#include "media/gpu/v4l2/v4l2_video_decoder_delegate_vp9.h"

namespace media {

namespace {

// Size of the timestamp cache, needs to be large enough for frame-reordering.
constexpr size_t kTimestampCacheSize = 128;
// Number of requests to allocate for submitting input buffers, if requests
// are used.

}  // namespace

struct V4L2StatelessVideoDecoderBackend::OutputRequest {
  static OutputRequest Surface(scoped_refptr<V4L2DecodeSurface> s,
                               base::TimeDelta t) {
    return OutputRequest(std::move(s), t);
  }

  static OutputRequest FlushFence() { return OutputRequest(kFlushFence); }

  static OutputRequest ChangeResolutionFence() {
    return OutputRequest(kChangeResolutionFence);
  }

  OutputRequest(const OutputRequest&) = delete;
  OutputRequest& operator=(const OutputRequest&) = delete;

  bool IsReady() const {
    return (type != OutputRequestType::kSurface) || surface->decoded();
  }

  // Allow move, but not copy.
  OutputRequest(OutputRequest&&) = default;

  enum OutputRequestType {
    // The surface to be outputted.
    kSurface,
    // The fence to indicate the flush request.
    kFlushFence,
    // The fence to indicate resolution change request.
    kChangeResolutionFence,
  };

  // The type of the request.
  const OutputRequestType type;
  // The surface to be outputted.
  scoped_refptr<V4L2DecodeSurface> surface;
  // The timestamp of the output frame. Because a surface might be outputted
  // multiple times with different timestamp, we need to store timestamp out of
  // surface.
  base::TimeDelta timestamp;

 private:
  OutputRequest(scoped_refptr<V4L2DecodeSurface> s, base::TimeDelta t)
      : type(kSurface), surface(std::move(s)), timestamp(t) {}
  explicit OutputRequest(OutputRequestType t) : type(t) {}
};

V4L2StatelessVideoDecoderBackend::DecodeRequest::DecodeRequest(
    scoped_refptr<DecoderBuffer> buf,
    VideoDecoder::DecodeCB cb,
    int32_t id)
    : buffer(std::move(buf)), decode_cb(std::move(cb)), bitstream_id(id) {}

V4L2StatelessVideoDecoderBackend::DecodeRequest::DecodeRequest(
    DecodeRequest&&) = default;
V4L2StatelessVideoDecoderBackend::DecodeRequest&
V4L2StatelessVideoDecoderBackend::DecodeRequest::operator=(DecodeRequest&&) =
    default;

V4L2StatelessVideoDecoderBackend::DecodeRequest::~DecodeRequest() = default;

V4L2StatelessVideoDecoderBackend::V4L2StatelessVideoDecoderBackend(
    Client* const client,
    scoped_refptr<V4L2Device> device,
    VideoCodecProfile profile,
    const VideoColorSpace& color_space,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    CdmContext* cdm_context)
    : V4L2VideoDecoderBackend(client, std::move(device)),
      profile_(profile),
      color_space_(color_space),
      bitstream_id_to_timestamp_(kTimestampCacheSize),
      task_runner_(task_runner),
      cdm_context_(cdm_context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  weak_this_ = weak_this_factory_.GetWeakPtr();
}

V4L2StatelessVideoDecoderBackend::~V4L2StatelessVideoDecoderBackend() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG_IF(WARNING, !surfaces_at_device_.empty())
      << "There is/are " << surfaces_at_device_.size()
      << " pending CAPTURE queue buffers pending dequeuing. This might be "
      << "fine or a problem depending on the destruction semantics (of the "
      << "client code).";

  if (!output_request_queue_.empty() || flush_cb_ || current_decode_request_ ||
      !decode_request_queue_.empty()) {
    VLOGF(1) << "Should not destroy backend during pending decode!";
  }
}

bool V4L2StatelessVideoDecoderBackend::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsSupportedProfile(profile_)) {
    VLOGF(1) << "Unsupported profile " << GetProfileName(profile_);
    return false;
  }

  if (!CreateDecoder())
    return false;

  CHECK(input_queue_->SupportsRequests());
  requests_queue_ = device_->GetRequestsQueue();
  return !!requests_queue_;
}

// static
void V4L2StatelessVideoDecoderBackend::ReuseOutputBufferThunk(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    std::optional<base::WeakPtr<V4L2StatelessVideoDecoderBackend>> weak_this,
    V4L2ReadableBufferRef buffer) {
  DVLOGF(3);
  DCHECK(weak_this);

  if (task_runner->RunsTasksInCurrentSequence()) {
    if (*weak_this) {
      (*weak_this)->ReuseOutputBuffer(std::move(buffer));
    }
  } else {
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&V4L2StatelessVideoDecoderBackend::ReuseOutputBuffer,
                       *weak_this, std::move(buffer)));
  }
}

void V4L2StatelessVideoDecoderBackend::ReuseOutputBuffer(
    V4L2ReadableBufferRef buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(3) << "Reuse output surface #" << buffer->BufferId();

  // Resume decoding in case of ran out of surface.
  if (pause_reason_ == PauseReason::kRanOutOfSurfaces) {
    pause_reason_ = PauseReason::kNone;
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&V4L2StatelessVideoDecoderBackend::DoDecodeWork,
                       weak_this_));
  }
}

void V4L2StatelessVideoDecoderBackend::OnOutputBufferDequeued(
    V4L2ReadableBufferRef dequeued_buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Mark the output buffer decoded, and try to output surface.
  DCHECK(!surfaces_at_device_.empty());
  auto surface = std::move(surfaces_at_device_.front());
  DCHECK_EQ(static_cast<size_t>(surface->output_record()),
            dequeued_buffer->BufferId());
  surfaces_at_device_.pop();

  surface->SetDecoded();

  auto reuse_buffer_cb =
      base::BindOnce(&V4L2StatelessVideoDecoderBackend::ReuseOutputBufferThunk,
                     task_runner_, weak_this_, std::move(dequeued_buffer));
  if (output_queue_->GetMemoryType() == V4L2_MEMORY_MMAP) {
    // Keep a reference to the V4L2 buffer until the frame is reused, because
    // the frame is backed up by the V4L2 buffer's memory.
    surface->frame()->AddDestructionObserver(std::move(reuse_buffer_cb));
  } else {
    // Keep a reference to the V4L2 buffer until the buffer is reused. The
    // reason for this is that the we currently use V4L2 buffer IDs to generate
    // timestamps to
    // reference frames, therefore we cannot reuse the same V4L2 buffer ID for
    // another decode operation until all references to that frame are gone.
    surface->SetReleaseCallback(std::move(reuse_buffer_cb));
  }

  PumpOutputSurfaces();
}

scoped_refptr<V4L2DecodeSurface>
V4L2StatelessVideoDecoderBackend::CreateSecureSurface(uint64_t secure_handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(4);

  // Request V4L2 input and output buffers.
  auto input_buf = input_queue_->GetFreeBuffer();
  auto output_buf = output_queue_->GetFreeBuffer();
  if (!input_buf || !output_buf) {
    DVLOGF(3) << "There is no free V4L2 buffer.";
    return nullptr;
  }

  DmabufVideoFramePool* pool = client_->GetVideoFramePool();
  scoped_refptr<FrameResource> frame;
  if (!pool) {
    // Get FrameResource from the V4L2 buffer because now we allocate from V4L2
    // driver via MMAP. The FrameResource received from V4L2 buffer will remain
    // until deallocating V4L2Queue. But we need to know when the buffer is not
    // used by the client. So we wrap the frame here.
    DCHECK_EQ(output_queue_->GetMemoryType(), V4L2_MEMORY_MMAP);
    scoped_refptr<FrameResource> origin_frame = output_buf->GetFrameResource();
    if (!origin_frame) {
      LOG(ERROR) << "There is no available FrameResource from the V4L2 buffer.";
      return nullptr;
    }

    frame = origin_frame->CreateWrappingFrame();
  } else {
    // This is used in cases when the video decoder format does not need
    // conversion before being sent to Chrome's Media pipeline. On ChromeOS,
    // currently only RK3399 (scarlet) supports this.
    DCHECK_EQ(output_queue_->GetMemoryType(), V4L2_MEMORY_DMABUF);
    frame = pool->GetFrame();
    if (!frame) {
      // We allocate the same number of output buffer slot in V4L2 device and
      // the output FrameResource. If there is free output buffer slot but no
      // free FrameResource, it means the FrameResource is not released at
      // client side. Post DoDecodeWork when the pool has available frames.
      DVLOGF(3) << "There is no available FrameResource.";
      pool->NotifyWhenFrameAvailable(base::BindOnce(
          base::IgnoreResult(&base::SequencedTaskRunner::PostTask),
          task_runner_, FROM_HERE,
          base::BindOnce(&V4L2StatelessVideoDecoderBackend::DoDecodeWork,
                         weak_this_)));
      return nullptr;
    }
  }

  scoped_refptr<V4L2DecodeSurface> dec_surface;
  CHECK(input_queue_->SupportsRequests());
  std::optional<V4L2RequestRef> request_ref = requests_queue_->GetFreeRequest();
  if (!request_ref) {
    DVLOGF(1) << "Could not get free request.";
    return nullptr;
  }

  return base::MakeRefCounted<V4L2RequestDecodeSurface>(
      std::move(*input_buf), std::move(*output_buf), std::move(frame),
      secure_handle, std::move(*request_ref));
}

scoped_refptr<V4L2DecodeSurface>
V4L2StatelessVideoDecoderBackend::CreateSurface() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(4);
  return CreateSecureSurface(0);
}

bool V4L2StatelessVideoDecoderBackend::SubmitSlice(
    V4L2DecodeSurface* dec_surface,
    const uint8_t* data,
    size_t size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(3);

  size_t plane_size = dec_surface->input_buffer().GetPlaneSize(0);
  size_t bytes_used = dec_surface->input_buffer().GetPlaneBytesUsed(0);
  if (size > plane_size - bytes_used) {
    LOG(ERROR) << "The size of submitted slice(" << size
               << ") is larger than the remaining buffer size("
               << plane_size - bytes_used << "). Plane size is " << plane_size;
    client_->OnBackendError();
    return false;
  }

  // Secure playback will submit a nullptr for |data|, the target data already
  // will exist in the secure buffer.
  if (data) {
    void* mapping = dec_surface->input_buffer().GetPlaneMapping(0);
    memcpy(reinterpret_cast<uint8_t*>(mapping) + bytes_used, data, size);
  }
  dec_surface->input_buffer().SetPlaneBytesUsed(0, bytes_used + size);
  return true;
}

void V4L2StatelessVideoDecoderBackend::DecodeSurface(
    scoped_refptr<V4L2DecodeSurface> dec_surface) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(3);

  DCHECK(current_decode_request_);
  const auto timestamp = current_decode_request_->buffer->timestamp();
  buffer_tracers_[timestamp] = std::make_unique<ScopedDecodeTrace>(
      "V4L2VideoDecoderBackendStateless", *(current_decode_request_->buffer));
  enqueuing_timestamps_[timestamp.InMilliseconds()] = base::TimeTicks::Now();

  if (!dec_surface->Submit()) {
    LOG(ERROR) << "Error while submitting frame for decoding!";
    client_->OnBackendError();
    return;
  }

  surfaces_at_device_.push(std::move(dec_surface));
}

void V4L2StatelessVideoDecoderBackend::SurfaceReady(
    scoped_refptr<V4L2DecodeSurface> dec_surface,
    int32_t bitstream_id,
    const gfx::Rect& visible_rect,
    const VideoColorSpace& color_space) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(3);

  // Find the timestamp associated with |bitstream_id|. It's possible that a
  // surface is output multiple times for different |bitstream_id|s (e.g. VP9
  // show_existing_frame feature). This means we need to output the same frame
  // again with a different timestamp.
  // On some rare occasions it's also possible that a single DecoderBuffer
  // produces multiple surfaces with the same |bitstream_id|, so we shouldn't
  // remove the timestamp from the cache.
  const auto it = bitstream_id_to_timestamp_.Peek(bitstream_id);
  CHECK(it != bitstream_id_to_timestamp_.end(), base::NotFatalUntil::M130);
  base::TimeDelta timestamp = it->second;

  dec_surface->SetVisibleRect(visible_rect);
  dec_surface->SetColorSpace(color_space);

  output_request_queue_.push(
      OutputRequest::Surface(std::move(dec_surface), timestamp));
  PumpOutputSurfaces();
}

void V4L2StatelessVideoDecoderBackend::ResumeDecoding() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DoDecodeWork();
}

void V4L2StatelessVideoDecoderBackend::EnqueueDecodeTask(
    scoped_refptr<DecoderBuffer> buffer,
    VideoDecoder::DecodeCB decode_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const int32_t bitstream_id =
      bitstream_id_generator_.GenerateNextId().GetUnsafeValue();

  if (!buffer->end_of_stream())
    bitstream_id_to_timestamp_.Put(bitstream_id, buffer->timestamp());

  decode_request_queue_.push(
      DecodeRequest(std::move(buffer), std::move(decode_cb), bitstream_id));

  // If we are already decoding, then we don't need to pump again.
  if (!current_decode_request_)
    DoDecodeWork();
}

void V4L2StatelessVideoDecoderBackend::DoDecodeWork() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!client_->IsDecoding())
    return;

  if (!PumpDecodeTask()) {
    LOG(ERROR) << "Failed to do decode work.";
    client_->OnBackendError();
  }
}

bool V4L2StatelessVideoDecoderBackend::PumpDecodeTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(3) << " Number of Decode requests: " << decode_request_queue_.size();

  pause_reason_ = PauseReason::kNone;
  while (true) {
    switch (decoder_->Decode()) {
      case AcceleratedVideoDecoder::kConfigChange:
        if (decoder_->GetBitDepth() != 8u && decoder_->GetBitDepth() != 10u) {
          VLOGF(2) << "Unsupported bit depth: "
                   << base::strict_cast<int>(decoder_->GetBitDepth());
          return false;
        }

        if (profile_ != decoder_->GetProfile()) {
          DVLOGF(3) << "Profile is changed: " << profile_ << " -> "
                    << decoder_->GetProfile();
          if (!IsSupportedProfile(decoder_->GetProfile())) {
            VLOGF(2) << "Unsupported profile: " << decoder_->GetProfile();
            return false;
          }

          profile_ = decoder_->GetProfile();
        }

        if (pic_size_ == decoder_->GetPicSize()) {
          // There is no need to do anything in V4L2 API when only a profile is
          // changed.
          DVLOGF(3) << "Only profile is changed. No need to do anything.";
          continue;
        }

        DVLOGF(3) << "Need to change resolution. Pause decoding.";
        client_->InitiateFlush();

        output_request_queue_.push(OutputRequest::ChangeResolutionFence());
        PumpOutputSurfaces();
        return true;

      case AcceleratedVideoDecoder::kRanOutOfStreamData:
        // Current decode request is finished processing.
        if (current_decode_request_) {
          std::move(current_decode_request_->decode_cb)
              .Run(DecoderStatus::Codes::kOk);
          current_decode_request_ = std::nullopt;
        }

        // Process next decode request.
        if (decode_request_queue_.empty())
          return true;
        current_decode_request_ = std::move(decode_request_queue_.front());
        decode_request_queue_.pop();

        if (current_decode_request_->buffer->end_of_stream()) {
          if (!decoder_->Flush()) {
            VLOGF(1) << "Failed flushing the decoder.";
            return false;
          }
          // Put the decoder in an idle state, ready to resume.
          decoder_->Reset();

          client_->InitiateFlush();
          DCHECK(!flush_cb_);
          flush_cb_ = std::move(current_decode_request_->decode_cb);

          output_request_queue_.push(OutputRequest::FlushFence());
          PumpOutputSurfaces();
          current_decode_request_ = std::nullopt;
          return true;
        }

        decoder_->SetStream(current_decode_request_->bitstream_id,
                            *current_decode_request_->buffer);
        break;

      case AcceleratedVideoDecoder::kRanOutOfSurfaces:
        DVLOGF(3) << "Ran out of surfaces. Resume when buffer is returned.";
        pause_reason_ = PauseReason::kRanOutOfSurfaces;
        return true;

      case AcceleratedVideoDecoder::kDecodeError:
        DVLOGF(3) << "Error decoding stream";
        return false;

      case AcceleratedVideoDecoder::kTryAgain:
        // In this case we are waiting for an async operation relating to secure
        // content. When that is complete, ResumeDecoding will be invoked and we
        // will start decoding again; or a reset will occur and that will resume
        // decoding.
        return true;
    }
  }
}

void V4L2StatelessVideoDecoderBackend::PumpOutputSurfaces() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(3) << "Number of display surfaces: " << output_request_queue_.size();

  bool resume_decode = false;
  while (!output_request_queue_.empty()) {
    if (!output_request_queue_.front().IsReady()) {
      DVLOGF(3) << "The first surface is not ready yet.";
      // It is possible that that V4L2 buffers for this output surface are not
      // even queued yet. Make sure that decoder_->Decode() is called to
      // continue that work and prevent the decoding thread from starving.
      resume_decode = true;
      break;
    }

    OutputRequest request = std::move(output_request_queue_.front());
    output_request_queue_.pop();
    switch (request.type) {
      case OutputRequest::kFlushFence:
        DCHECK(output_request_queue_.empty());
        DVLOGF(2) << "Flush finished.";
        std::move(flush_cb_).Run(DecoderStatus::Codes::kOk);
        resume_decode = true;
        client_->CompleteFlush();
        break;

      case OutputRequest::kChangeResolutionFence:
        DCHECK(output_request_queue_.empty());
        ChangeResolution();
        break;

      case OutputRequest::kSurface:
        scoped_refptr<V4L2DecodeSurface> surface = std::move(request.surface);

        DCHECK(surface->frame());
        client_->OutputFrame(surface->frame(), surface->visible_rect(),
                             surface->color_space(), request.timestamp);

        {
          const auto timestamp = surface->frame()->timestamp();
          const auto flat_timestamp = timestamp.InMilliseconds();
          // TODO(b/190615065) |flat_timestamp| might be repeated with H.264
          // bitstreams, investigate why, and change the if() to DCHECK().
          if (base::Contains(enqueuing_timestamps_, flat_timestamp)) {
            const auto decoding_begin = enqueuing_timestamps_[flat_timestamp];
            const auto decoding_end = base::TimeTicks::Now();
            UMA_HISTOGRAM_TIMES("Media.PlatformVideoDecoding.Decode",
                                decoding_end - decoding_begin);
            enqueuing_timestamps_.erase(flat_timestamp);
          }

          auto iter = buffer_tracers_.find(timestamp);
          if (iter != buffer_tracers_.end()) {
            iter->second->EndTrace(DecoderStatus::Codes::kOk);
            buffer_tracers_.erase(iter);
          }
        }

        break;
    }
  }

  if (resume_decode) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&V4L2StatelessVideoDecoderBackend::DoDecodeWork,
                       weak_this_));
  }
}

void V4L2StatelessVideoDecoderBackend::ChangeResolution() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // We change resolution after outputting all pending surfaces, there should
  // be no V4L2DecodeSurface left.
  DCHECK(surfaces_at_device_.empty());
  DCHECK(output_request_queue_.empty());

  const size_t num_codec_reference_frames = decoder_->GetNumReferenceFrames();
  // Verify |num_codec_reference_frames| has a reasonable value. Anecdotally 16
  // is the largest amount of reference frames seen, on an ITU-T H.264 test
  // vector (CAPCM*1_Sand_E.h264).
  CHECK_LE(num_codec_reference_frames, 32u);

  const gfx::Rect visible_rect = decoder_->GetVisibleRect();
  const gfx::Size pic_size = decoder_->GetPicSize();
  const uint8_t bit_depth = decoder_->GetBitDepth();

  // Set output format with the new resolution.
  DCHECK(!pic_size.IsEmpty());
  DVLOGF(3) << "Change resolution to " << pic_size.ToString();
  client_->ChangeResolution(pic_size, visible_rect, num_codec_reference_frames,
                            bit_depth);
}

bool V4L2StatelessVideoDecoderBackend::ApplyResolution(
    const gfx::Size& pic_size,
    const gfx::Rect& visible_rect) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(input_queue_->QueuedBuffersCount(), 0u);

  auto ret = input_queue_->GetFormat().first;
  if (!ret) {
    VPLOGF(1) << "Failed getting OUTPUT format";
    return false;
  }
  struct v4l2_format format = std::move(*ret);

  format.fmt.pix_mp.width = pic_size.width();
  format.fmt.pix_mp.height = pic_size.height();
  if (device_->Ioctl(VIDIOC_S_FMT, &format) != 0) {
    RecordVidiocIoctlErrorUMA(VidiocIoctlRequests::kVidiocSFmt);
    VPLOGF(1) << "Failed setting OUTPUT format";
    return false;
  }

  return true;
}

void V4L2StatelessVideoDecoderBackend::OnChangeResolutionDone(
    CroStatus status) {
  if (status == CroStatus::Codes::kResetRequired)
    return;

  if (status != CroStatus::Codes::kOk) {
    LOG(ERROR) << "Backend failure when changing resolution ("
               << static_cast<int>(status.code()) << ").";
    client_->OnBackendError();
    return;
  }

  pic_size_ = decoder_->GetPicSize();
  client_->CompleteFlush();
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&V4L2StatelessVideoDecoderBackend::DoDecodeWork,
                                weak_this_));
}

void V4L2StatelessVideoDecoderBackend::OnStreamStopped(bool stop_input_queue) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(3);

  // The V4L2 stream has been stopped stopped, so all surfaces on the device
  // have been returned to the client.
  surfaces_at_device_ = {};
}

void V4L2StatelessVideoDecoderBackend::ClearPendingRequests(
    DecoderStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(3);

  if (decoder_) {
    // If we reset during resolution change, re-create AVD. Then the new AVD
    // will trigger resolution change again after reset.
    if (pic_size_ != decoder_->GetPicSize()) {
      CreateDecoder();
    } else {
      decoder_->Reset();
    }
  }

  // Clear output_request_queue_.
  while (!output_request_queue_.empty())
    output_request_queue_.pop();

  if (flush_cb_)
    std::move(flush_cb_).Run(status);

  // Clear current_decode_request_ and decode_request_queue_.
  if (current_decode_request_) {
    std::move(current_decode_request_->decode_cb).Run(status);
    current_decode_request_ = std::nullopt;
  }

  while (!decode_request_queue_.empty()) {
    auto request = std::move(decode_request_queue_.front());
    decode_request_queue_.pop();
    std::move(request.decode_cb).Run(status);
  }
}

bool V4L2StatelessVideoDecoderBackend::StopInputQueueOnResChange() const {
  return true;
}

size_t V4L2StatelessVideoDecoderBackend::GetNumOUTPUTQueueBuffers(
    bool secure_mode) const {
  // Some H.264 test vectors (CAPCM*1_Sand_E.h264) need 16 reference frames; add
  // one to calculate the number of OUTPUT buffers, to account for the frame
  // being decoded.
  // For secure mode, we are very memory constrained so only allocate 8 buffers.
  // TODO(b/249325255): reduce this number to e.g. 8 or even less when it does
  // not artificially limit the size of the CAPTURE (decoded video frames)
  // queue.
  constexpr size_t kNumInputBuffers = 16 + 1;
  constexpr size_t kNumInputBuffersSecureMode = 8;
  return secure_mode ? kNumInputBuffersSecureMode : kNumInputBuffers;
}

bool V4L2StatelessVideoDecoderBackend::IsSupportedProfile(
    VideoCodecProfile profile) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(device_);
  if (supported_profiles_.empty()) {
    const std::vector<uint32_t> kSupportedInputFourccs = {
      V4L2_PIX_FMT_H264_SLICE,
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
      V4L2_PIX_FMT_HEVC_SLICE,
#endif
      V4L2_PIX_FMT_VP8_FRAME,
      V4L2_PIX_FMT_VP9_FRAME,
      V4L2_PIX_FMT_AV1_FRAME,
    };
    auto device = base::MakeRefCounted<V4L2Device>();
    VideoDecodeAccelerator::SupportedProfiles profiles =
        device->GetSupportedDecodeProfiles(kSupportedInputFourccs);
    for (const auto& entry : profiles)
      supported_profiles_.push_back(entry.profile);
  }
  return base::Contains(supported_profiles_, profile);
}

bool V4L2StatelessVideoDecoderBackend::CreateDecoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(3);

  pic_size_ = gfx::Size();

  CHECK(input_queue_->SupportsRequests());

  if (profile_ >= H264PROFILE_MIN && profile_ <= H264PROFILE_MAX) {
    decoder_ = std::make_unique<H264Decoder>(
        std::make_unique<V4L2VideoDecoderDelegateH264>(this, device_.get(),
                                                       cdm_context_),
        profile_, color_space_);
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
  } else if (profile_ >= HEVCPROFILE_MIN && profile_ <= HEVCPROFILE_MAX) {
    decoder_ = std::make_unique<H265Decoder>(
        std::make_unique<V4L2VideoDecoderDelegateH265>(this, device_.get()),
        profile_, color_space_);
#endif
  } else if (profile_ >= VP8PROFILE_MIN && profile_ <= VP8PROFILE_MAX) {
    decoder_ = std::make_unique<VP8Decoder>(
        std::make_unique<V4L2VideoDecoderDelegateVP8>(this, device_.get()),
        color_space_);
  } else if (profile_ >= VP9PROFILE_MIN && profile_ <= VP9PROFILE_MAX) {
    decoder_ = std::make_unique<VP9Decoder>(
        std::make_unique<V4L2VideoDecoderDelegateVP9>(this, device_.get()),
        profile_, color_space_);
#if BUILDFLAG(IS_CHROMEOS)
  } else if (profile_ >= AV1PROFILE_MIN && profile_ <= AV1PROFILE_MAX) {
    decoder_ = std::make_unique<AV1Decoder>(
        std::make_unique<V4L2VideoDecoderDelegateAV1>(this, device_.get()),
        profile_, color_space_);
#endif
  } else {
    VLOGF(1) << "Unsupported profile " << GetProfileName(profile_);
    return false;
  }
  return true;
}

}  // namespace media

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/v4l2_video_decoder_backend_stateless.h"

#include <fcntl.h>
#include <linux/media.h>
#include <sys/ioctl.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/posix/eintr_wrapper.h"
#include "base/sequenced_task_runner.h"
#include "media/base/decode_status.h"
#include "media/base/video_codecs.h"
#include "media/base/video_frame.h"
#include "media/gpu/accelerated_video_decoder.h"
#include "media/gpu/macros.h"
#include "media/gpu/v4l2/v4l2_device.h"
#include "media/gpu/v4l2/v4l2_h264_accelerator.h"
#include "media/gpu/v4l2/v4l2_h264_accelerator_legacy.h"
#include "media/gpu/v4l2/v4l2_vp8_accelerator.h"
#include "media/gpu/v4l2/v4l2_vp8_accelerator_legacy.h"
#include "media/gpu/v4l2/v4l2_vp9_accelerator.h"

namespace media {

namespace {

// Size of the timestamp cache, needs to be large enough for frame-reordering.
constexpr size_t kTimestampCacheSize = 128;
// Number of requests to allocate for submitting input buffers, if requests
// are used.
constexpr size_t kNumRequests = 16;

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

  DISALLOW_COPY_AND_ASSIGN(OutputRequest);
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
    DmabufVideoFramePool* const frame_pool,
    VideoCodecProfile profile,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : V4L2VideoDecoderBackend(client, std::move(device)),
      frame_pool_(frame_pool),
      profile_(profile),
      bitstream_id_to_timestamp_(kTimestampCacheSize),
      task_runner_(task_runner) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  weak_this_ = weak_this_factory_.GetWeakPtr();
}

V4L2StatelessVideoDecoderBackend::~V4L2StatelessVideoDecoderBackend() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(surfaces_at_device_.empty());

  if (!output_request_queue_.empty() || flush_cb_ || current_decode_request_ ||
      !decode_request_queue_.empty()) {
    VLOGF(1) << "Should not destroy backend during pending decode!";
  }

  if (avd_) {
    avd_->Reset();
    avd_ = nullptr;
  }

  if (supports_requests_) {
    requests_ = {};
    media_fd_.reset();
  }
}

bool V4L2StatelessVideoDecoderBackend::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!CheckRequestAPISupport()) {
    VPLOGF(1) << "Failed to check request api support.";
    return false;
  }

  // Create codec-specific AcceleratedVideoDecoder.
  // TODO(akahuang): Check the profile is supported.
  if (profile_ >= H264PROFILE_MIN && profile_ <= H264PROFILE_MAX) {
    if (supports_requests_) {
      avd_.reset(new H264Decoder(
          std::make_unique<V4L2H264Accelerator>(this, device_.get())));
    } else {
      avd_.reset(new H264Decoder(
          std::make_unique<V4L2LegacyH264Accelerator>(this, device_.get())));
    }
  } else if (profile_ >= VP8PROFILE_MIN && profile_ <= VP8PROFILE_MAX) {
    if (supports_requests_) {
      avd_.reset(new VP8Decoder(
          std::make_unique<V4L2VP8Accelerator>(this, device_.get())));
    } else {
      avd_.reset(new VP8Decoder(
          std::make_unique<V4L2LegacyVP8Accelerator>(this, device_.get())));
    }
  } else if (profile_ >= VP9PROFILE_MIN && profile_ <= VP9PROFILE_MAX) {
    avd_.reset(new VP9Decoder(
        std::make_unique<V4L2VP9Accelerator>(this, device_.get())));
  } else {
    VLOGF(1) << "Unsupported profile " << GetProfileName(profile_);
    return false;
  }

  if (supports_requests_ && !AllocateRequests()) {
    return false;
  }

  return true;
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

  // Keep a reference to the V4L2 buffer until the buffer is reused. The
  // reason for this is that the config store uses V4L2 buffer IDs to
  // reference frames, therefore we cannot reuse the same V4L2 buffer ID for
  // another decode operation until all references to that frame are gone.
  // Request API does not have this limitation, so we can probably remove this
  // after config store is gone.
  surface->SetReleaseCallback(
      base::BindOnce(&V4L2StatelessVideoDecoderBackend::ReuseOutputBuffer,
                     weak_this_, std::move(dequeued_buffer)));

  PumpOutputSurfaces();

  // If we were waiting for an output buffer to be available, schedule a
  // decode task.
  if (pause_reason_ == PauseReason::kWaitSubFrameDecoded) {
    pause_reason_ = PauseReason::kNone;
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&V4L2StatelessVideoDecoderBackend::DoDecodeWork,
                       weak_this_));
  }
}

scoped_refptr<V4L2DecodeSurface>
V4L2StatelessVideoDecoderBackend::CreateSurface() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(4);

  // Request VideoFrame.
  scoped_refptr<VideoFrame> frame = frame_pool_->GetFrame();
  if (!frame) {
    // We allocate the same number of output buffer slot in V4L2 device and the
    // output VideoFrame. If there is free output buffer slot but no free
    // VideoFrame, surface_it means the VideoFrame is not released at client
    // side. Post DoDecodeWork when the pool has available frames.
    DVLOGF(3) << "There is no available VideoFrame.";
    frame_pool_->NotifyWhenFrameAvailable(base::BindOnce(
        base::IgnoreResult(&base::SequencedTaskRunner::PostTask), task_runner_,
        FROM_HERE,
        base::BindOnce(&V4L2StatelessVideoDecoderBackend::DoDecodeWork,
                       weak_this_)));
    return nullptr;
  }

  // Request V4L2 input and output buffers.
  V4L2WritableBufferRef input_buf = input_queue_->GetFreeBuffer();
  V4L2WritableBufferRef output_buf = output_queue_->GetFreeBuffer();
  if (!input_buf.IsValid() || !output_buf.IsValid()) {
    DVLOGF(3) << "There is no free V4L2 buffer.";
    return nullptr;
  }

  scoped_refptr<V4L2DecodeSurface> dec_surface;
  if (supports_requests_) {
    DCHECK(!requests_.empty());
    base::ScopedFD request = std::move(requests_.front());
    requests_.pop();
    auto ret = V4L2RequestDecodeSurface::Create(
        std::move(input_buf), std::move(output_buf), std::move(frame),
        request.get());
    requests_.push(std::move(request));
    if (!ret) {
      DVLOGF(3) << "Could not create surface.";
      return nullptr;
    }
    dec_surface = std::move(*ret);
  } else {
    dec_surface = new V4L2ConfigStoreDecodeSurface(
        std::move(input_buf), std::move(output_buf), std::move(frame));
  }

  return dec_surface;
}

bool V4L2StatelessVideoDecoderBackend::SubmitSlice(
    const scoped_refptr<V4L2DecodeSurface>& dec_surface,
    const uint8_t* data,
    size_t size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(3);

  size_t plane_size = dec_surface->input_buffer().GetPlaneSize(0);
  size_t bytes_used = dec_surface->input_buffer().GetPlaneBytesUsed(0);
  if (size > plane_size - bytes_used) {
    VLOGF(1) << "The size of submitted slice(" << size
             << ") is larger than the remaining buffer size("
             << plane_size - bytes_used << "). Plane size is " << plane_size;
    client_->OnBackendError();
    return false;
  }

  void* mapping = dec_surface->input_buffer().GetPlaneMapping(0);
  memcpy(reinterpret_cast<uint8_t*>(mapping) + bytes_used, data, size);
  dec_surface->input_buffer().SetPlaneBytesUsed(0, bytes_used + size);
  return true;
}

void V4L2StatelessVideoDecoderBackend::DecodeSurface(
    const scoped_refptr<V4L2DecodeSurface>& dec_surface) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(3);

  // Enqueue input_buf and output_buf
  dec_surface->input_buffer().PrepareQueueBuffer(*dec_surface);

  if (!std::move(dec_surface->input_buffer()).QueueMMap()) {
    client_->OnBackendError();
    return;
  }

  if (!std::move(dec_surface->output_buffer())
           .QueueDMABuf(dec_surface->video_frame()->DmabufFds())) {
    client_->OnBackendError();
    return;
  }

  if (!dec_surface->Submit()) {
    VLOGF(1) << "Error while submitting frame for decoding!";
    client_->OnBackendError();
    return;
  }

  surfaces_at_device_.push(std::move(dec_surface));
}

void V4L2StatelessVideoDecoderBackend::SurfaceReady(
    const scoped_refptr<V4L2DecodeSurface>& dec_surface,
    int32_t bitstream_id,
    const gfx::Rect& visible_rect,
    const VideoColorSpace& /* color_space */) {
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
  DCHECK(it != bitstream_id_to_timestamp_.end());
  base::TimeDelta timestamp = it->second;

  dec_surface->SetVisibleRect(visible_rect);
  output_request_queue_.push(
      OutputRequest::Surface(std::move(dec_surface), timestamp));
  PumpOutputSurfaces();
}

void V4L2StatelessVideoDecoderBackend::EnqueueDecodeTask(
    scoped_refptr<DecoderBuffer> buffer,
    VideoDecoder::DecodeCB decode_cb,
    int32_t bitstream_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!buffer->end_of_stream()) {
    bitstream_id_to_timestamp_.Put(bitstream_id, buffer->timestamp());
  }

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

  if (!PumpDecodeTask())
    client_->OnBackendError();
}

bool V4L2StatelessVideoDecoderBackend::PumpDecodeTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(3) << " Number of Decode requests: " << decode_request_queue_.size();

  pause_reason_ = PauseReason::kNone;
  while (true) {
    switch (avd_->Decode()) {
      case AcceleratedVideoDecoder::kAllocateNewSurfaces:
        DVLOGF(3) << "Need to change resolution. Pause decoding.";
        client_->InitiateFlush();

        output_request_queue_.push(OutputRequest::ChangeResolutionFence());
        PumpOutputSurfaces();
        return true;

      case AcceleratedVideoDecoder::kRanOutOfStreamData:
        // Current decode request is finished processing.
        if (current_decode_request_) {
          DCHECK(current_decode_request_->decode_cb);
          std::move(current_decode_request_->decode_cb).Run(DecodeStatus::OK);
          current_decode_request_ = base::nullopt;
        }

        // Process next decode request.
        if (decode_request_queue_.empty())
          return true;
        current_decode_request_ = std::move(decode_request_queue_.front());
        decode_request_queue_.pop();

        if (current_decode_request_->buffer->end_of_stream()) {
          if (!avd_->Flush()) {
            VLOGF(1) << "Failed flushing the decoder.";
            return false;
          }
          // Put the decoder in an idle state, ready to resume.
          avd_->Reset();

          client_->InitiateFlush();
          DCHECK(!flush_cb_);
          flush_cb_ = std::move(current_decode_request_->decode_cb);

          output_request_queue_.push(OutputRequest::FlushFence());
          PumpOutputSurfaces();
          current_decode_request_ = base::nullopt;
          return true;
        }

        avd_->SetStream(current_decode_request_->bitstream_id,
                        *current_decode_request_->buffer);
        break;

      case AcceleratedVideoDecoder::kRanOutOfSurfaces:
        DVLOGF(3) << "Ran out of surfaces. Resume when buffer is returned.";
        pause_reason_ = PauseReason::kRanOutOfSurfaces;
        return true;

      case AcceleratedVideoDecoder::kNeedContextUpdate:
        DVLOGF(3) << "Awaiting context update";
        pause_reason_ = PauseReason::kWaitSubFrameDecoded;
        return true;

      case AcceleratedVideoDecoder::kDecodeError:
        DVLOGF(3) << "Error decoding stream";
        return false;

      case AcceleratedVideoDecoder::kTryAgain:
        NOTREACHED() << "Should not reach here unless this class accepts "
                        "encrypted streams.";
        DVLOGF(4) << "No key for decoding stream.";
        return false;
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
      break;
    }

    OutputRequest request = std::move(output_request_queue_.front());
    output_request_queue_.pop();
    switch (request.type) {
      case OutputRequest::kFlushFence:
        DCHECK(output_request_queue_.empty());
        DVLOGF(2) << "Flush finished.";
        std::move(flush_cb_).Run(DecodeStatus::OK);
        resume_decode = true;
        break;

      case OutputRequest::kChangeResolutionFence:
        DCHECK(output_request_queue_.empty());
        if (!ChangeResolution()) {
          client_->OnBackendError();
          return;
        }
        resume_decode = true;
        break;

      case OutputRequest::kSurface:
        scoped_refptr<V4L2DecodeSurface> surface = std::move(request.surface);

        DCHECK(surface->video_frame());
        client_->OutputFrame(surface->video_frame(), surface->visible_rect(),
                             request.timestamp);
        break;
    }
  }

  if (resume_decode) {
    client_->CompleteFlush();
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&V4L2StatelessVideoDecoderBackend::DoDecodeWork,
                       weak_this_));
  }
}

bool V4L2StatelessVideoDecoderBackend::ChangeResolution() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // We change resolution after outputting all pending surfaces, there should
  // be no V4L2DecodeSurface left.
  DCHECK(surfaces_at_device_.empty());
  DCHECK(output_request_queue_.empty());
  // Set output format with the new resolution.
  gfx::Size pic_size = avd_->GetPicSize();
  DCHECK(!pic_size.IsEmpty());
  DVLOGF(3) << "Change resolution to " << pic_size.ToString();

  size_t num_output_frames = avd_->GetRequiredNumOfPictures();
  gfx::Rect visible_rect = avd_->GetVisibleRect();
  return client_->ChangeResolution(pic_size, visible_rect, num_output_frames);
}

void V4L2StatelessVideoDecoderBackend::OnStreamStopped() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(3);

  // The V4L2 stream has been stopped stopped, so all surfaces on the device
  // have been returned to the client.
  surfaces_at_device_ = {};
}

void V4L2StatelessVideoDecoderBackend::ClearPendingRequests(
    DecodeStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(3);

  if (avd_)
    avd_->Reset();

  // Clear output_request_queue_.
  while (!output_request_queue_.empty())
    output_request_queue_.pop();

  if (flush_cb_)
    std::move(flush_cb_).Run(status);

  // Clear current_decode_request_ and decode_request_queue_.
  if (current_decode_request_) {
    std::move(current_decode_request_->decode_cb).Run(status);
    current_decode_request_ = base::nullopt;
  }

  while (!decode_request_queue_.empty()) {
    auto request = std::move(decode_request_queue_.front());
    decode_request_queue_.pop();
    std::move(request.decode_cb).Run(status);
  }
}

bool V4L2StatelessVideoDecoderBackend::CheckRequestAPISupport() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(3);

  struct v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof(reqbufs));
  reqbufs.count = 0;
  reqbufs.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  reqbufs.memory = V4L2_MEMORY_MMAP;
  if (device_->Ioctl(VIDIOC_REQBUFS, &reqbufs) != 0) {
    VPLOGF(1) << "VIDIOC_REQBUFS ioctl failed.";
    return false;
  }
  if (reqbufs.capabilities & V4L2_BUF_CAP_SUPPORTS_REQUESTS) {
    supports_requests_ = true;
    VLOGF(1) << "Using request API.";
    DCHECK(!media_fd_.is_valid());
    // Let's try to open the media device
    // TODO(crbug.com/985230): remove this hardcoding, replace with V4L2Device
    // integration.
    int media_fd = open("/dev/media-dec0", O_RDWR, 0);
    if (media_fd < 0) {
      VPLOGF(1) << "Failed to open media device.";
      return false;
    }
    media_fd_ = base::ScopedFD(media_fd);
  } else {
    VLOGF(1) << "Using config store.";
  }

  return true;
}

bool V4L2StatelessVideoDecoderBackend::AllocateRequests() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(3);

  DCHECK(requests_.empty());

  for (size_t i = 0; i < kNumRequests; i++) {
    int request_fd;

    int ret = HANDLE_EINTR(
        ioctl(media_fd_.get(), MEDIA_IOC_REQUEST_ALLOC, &request_fd));
    if (ret < 0) {
      VPLOGF(1) << "Failed to create request: ";
      return false;
    }

    requests_.push(base::ScopedFD(request_fd));
  }
  DCHECK_EQ(requests_.size(), kNumRequests);

  return true;
}

}  // namespace media

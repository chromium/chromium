// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_video_decoder.h"

#include <limits>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/format_utils.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/gpu/chromeos/dmabuf_video_frame_pool.h"
#include "media/gpu/gpu_video_decode_accelerator_helpers.h"
#include "media/gpu/macros.h"
#include "media/gpu/vaapi/va_surface.h"
#include "media/gpu/vaapi/vaapi_h264_accelerator.h"
#include "media/gpu/vaapi/vaapi_vp8_accelerator.h"
#include "media/gpu/vaapi/vaapi_vp9_accelerator.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"

namespace media {

namespace {

// Size of the timestamp cache, needs to be large enough for frame-reordering.
constexpr size_t kTimestampCacheSize = 128;

// Returns the preferred VA_RT_FORMAT for the given |profile|.
unsigned int GetVaFormatForVideoCodecProfile(VideoCodecProfile profile) {
  switch (profile) {
    case VP9PROFILE_PROFILE2:
    case VP9PROFILE_PROFILE3:
      return VA_RT_FORMAT_YUV420_10BPP;
    default:
      return VA_RT_FORMAT_YUV420;
  }
}

gfx::BufferFormat GetBufferFormat() {
#if defined(USE_OZONE)
  return gfx::BufferFormat::YUV_420_BIPLANAR;
#else
  return gfx::BufferFormat::RGBX_8888;
#endif
}

}  // namespace

VaapiVideoDecoder::DecodeTask::DecodeTask(scoped_refptr<DecoderBuffer> buffer,
                                          int32_t buffer_id,
                                          DecodeCB decode_done_cb)
    : buffer_(std::move(buffer)),
      buffer_id_(buffer_id),
      decode_done_cb_(std::move(decode_done_cb)) {}

VaapiVideoDecoder::DecodeTask::~DecodeTask() = default;

VaapiVideoDecoder::DecodeTask::DecodeTask(DecodeTask&&) = default;

// static
std::unique_ptr<VideoDecoderPipeline::DecoderInterface>
VaapiVideoDecoder::Create(
    scoped_refptr<base::SequencedTaskRunner> decoder_task_runner,
    GetFramePoolCB get_pool_cb) {
  return base::WrapUnique<VideoDecoderPipeline::DecoderInterface>(
      new VaapiVideoDecoder(std::move(decoder_task_runner),
                            std::move(get_pool_cb)));
}

// static
SupportedVideoDecoderConfigs VaapiVideoDecoder::GetSupportedConfigs() {
  return ConvertFromSupportedProfiles(
      VaapiWrapper::GetSupportedDecodeProfiles(), false);
}

VaapiVideoDecoder::VaapiVideoDecoder(
    scoped_refptr<base::SequencedTaskRunner> decoder_task_runner,
    GetFramePoolCB get_pool_cb)
    : get_pool_cb_(std::move(get_pool_cb)),
      buffer_id_to_timestamp_(kTimestampCacheSize),
      decoder_task_runner_(std::move(decoder_task_runner)),
      weak_this_factory_(this) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  VLOGF(2);

  weak_this_ = weak_this_factory_.GetWeakPtr();
}

VaapiVideoDecoder::~VaapiVideoDecoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  VLOGF(2);

  // Abort all currently scheduled decode tasks.
  ClearDecodeTaskQueue(DecodeStatus::ABORTED);

  if (decoder_) {
    decoder_->Reset();
    decoder_ = nullptr;
  }

  weak_this_factory_.InvalidateWeakPtrs();
}

void VaapiVideoDecoder::Initialize(const VideoDecoderConfig& config,
                                   InitCB init_cb,
                                   const OutputCB& output_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DCHECK(config.IsValidConfig());
  DCHECK(state_ == State::kUninitialized || state_ == State::kWaitingForInput);
  DVLOGF(3);

  // Reinitializing the decoder is allowed if there are no pending decodes.
  if (current_decode_task_ || !decode_task_queue_.empty()) {
    VLOGF(1) << "Don't call Initialize() while there are pending decode tasks";
    std::move(init_cb).Run(false);
    return;
  }

  // We expect the decoder to have released all output buffers (by the client
  // triggering a flush or reset), even if the
  // VideoDecoderPipeline::DecoderInterface API doesn't explicitly specify this.
  DCHECK(output_frames_.empty());

  if (state_ != State::kUninitialized) {
    DVLOGF(3) << "Reinitializing decoder";
    if (decoder_) {
      decoder_->Reset();
      decoder_ = nullptr;
    }
    vaapi_wrapper_ = nullptr;
    SetState(State::kUninitialized);
  }

  // Initialize VAAPI wrapper.
  VideoCodecProfile profile = config.profile();
  vaapi_wrapper_ = VaapiWrapper::CreateForVideoCodec(
      VaapiWrapper::kDecode, profile, base::DoNothing());
  if (!vaapi_wrapper_.get()) {
    VLOGF(1) << "Failed initializing VAAPI for profile "
             << GetProfileName(profile);
    std::move(init_cb).Run(false);
    return;
  }

  // Create AcceleratedVideoDecoder for the specified profile.
  if (profile >= H264PROFILE_MIN && profile <= H264PROFILE_MAX) {
    decoder_.reset(new H264Decoder(
        std::make_unique<VaapiH264Accelerator>(this, vaapi_wrapper_),
        config.color_space_info()));
  } else if (profile >= VP8PROFILE_MIN && profile <= VP8PROFILE_MAX) {
    decoder_.reset(new VP8Decoder(
        std::make_unique<VaapiVP8Accelerator>(this, vaapi_wrapper_)));
  } else if (profile >= VP9PROFILE_MIN && profile <= VP9PROFILE_MAX) {
    decoder_.reset(new VP9Decoder(
        std::make_unique<VaapiVP9Accelerator>(this, vaapi_wrapper_),
        config.color_space_info()));
  } else {
    VLOGF(1) << "Unsupported profile " << GetProfileName(profile);
    std::move(init_cb).Run(false);
    return;
  }

  // Get and initialize the frame pool.
  frame_pool_ = get_pool_cb_.Run();

  pixel_aspect_ratio_ = config.GetPixelAspectRatio();
  profile_ = profile;

  output_cb_ = std::move(output_cb);
  SetState(State::kWaitingForInput);

  // Notify client initialization was successful.
  std::move(init_cb).Run(true);
}

void VaapiVideoDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                               DecodeCB decode_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(4) << "Queuing input buffer, id: " << next_buffer_id_ << ", size: "
            << (buffer->end_of_stream() ? 0 : buffer->data_size());

  // If we're in the error state, immediately fail the decode task.
  if (state_ == State::kError) {
    std::move(decode_cb).Run(DecodeStatus::DECODE_ERROR);
    return;
  }

  if (!buffer->end_of_stream())
    buffer_id_to_timestamp_.Put(next_buffer_id_, buffer->timestamp());

  decode_task_queue_.emplace(std::move(buffer), next_buffer_id_,
                             std::move(decode_cb));

  // Generate the next positive buffer id.
  next_buffer_id_ = (next_buffer_id_ + 1) & 0x7fffffff;

  // If we were waiting for input buffers, start decoding again.
  if (state_ == State::kWaitingForInput) {
    DCHECK(!current_decode_task_);
    SetState(State::kDecoding);
    ScheduleNextDecodeTask();
  }
}

void VaapiVideoDecoder::ScheduleNextDecodeTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DCHECK_EQ(state_, State::kDecoding);
  DCHECK(!current_decode_task_);
  DCHECK(!decode_task_queue_.empty());

  // Dequeue the next decode task.
  current_decode_task_ = std::move(decode_task_queue_.front());
  decode_task_queue_.pop();
  if (!current_decode_task_->buffer_->end_of_stream()) {
    decoder_->SetStream(current_decode_task_->buffer_id_,
                        *current_decode_task_->buffer_);
  }

  decoder_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VaapiVideoDecoder::HandleDecodeTask, weak_this_));
}

void VaapiVideoDecoder::HandleDecodeTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(4);

  if (state_ == State::kError || state_ == State::kResetting)
    return;

  DCHECK_EQ(state_, State::kDecoding);
  DCHECK(current_decode_task_);

  // Check whether a flush was requested.
  if (current_decode_task_->buffer_->end_of_stream()) {
    FlushTask();
    return;
  }

  AcceleratedVideoDecoder::DecodeResult decode_result = decoder_->Decode();
  switch (decode_result) {
    case AcceleratedVideoDecoder::kRanOutOfStreamData:
      // Decoding was successful, notify client and try to schedule the next
      // task. Switch to the idle state if we ran out of buffers to decode.
      std::move(current_decode_task_->decode_done_cb_).Run(DecodeStatus::OK);
      current_decode_task_ = base::nullopt;
      if (!decode_task_queue_.empty()) {
        ScheduleNextDecodeTask();
      } else {
        SetState(State::kWaitingForInput);
      }
      break;
    case AcceleratedVideoDecoder::kAllocateNewSurfaces:
      // A new set of output buffers is requested. We either didn't have any
      // output buffers yet or encountered a resolution change.
      ChangeFrameResolutionTask();
      break;
    case AcceleratedVideoDecoder::kRanOutOfSurfaces:
      // No more surfaces to decode into available, wait until client returns
      // video frames to the frame pool.
      SetState(State::kWaitingForOutput);
      break;
    case AcceleratedVideoDecoder::kNeedContextUpdate:
      DVLOGF(3) << "Context updates not supported";
      SetState(State::kError);
      break;
    case AcceleratedVideoDecoder::kDecodeError:
      DVLOGF(3) << "Error decoding stream";
      SetState(State::kError);
      break;
    case AcceleratedVideoDecoder::kTryAgain:
      DVLOGF(3) << "Encrypted streams not supported";
      SetState(State::kError);
      break;
  }
}

void VaapiVideoDecoder::ClearDecodeTaskQueue(DecodeStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(4);

  if (current_decode_task_) {
    std::move(current_decode_task_->decode_done_cb_).Run(status);
    current_decode_task_ = base::nullopt;
  }

  while (!decode_task_queue_.empty()) {
    std::move(decode_task_queue_.front().decode_done_cb_).Run(status);
    decode_task_queue_.pop();
  }
}

scoped_refptr<VASurface> VaapiVideoDecoder::CreateSurface() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DCHECK_EQ(state_, State::kDecoding);
  DCHECK(current_decode_task_);
  DVLOGF(4);

  // Get a video frame from the video frame pool.
  scoped_refptr<VideoFrame> frame = frame_pool_->GetFrame();
  if (!frame) {
    // Ask the video frame pool to notify us when new frames are available, so
    // we can retry the current decode task.
    frame_pool_->NotifyWhenFrameAvailable(base::BindOnce(
        &VaapiVideoDecoder::NotifyFrameAvailableTask, weak_this_));
    return nullptr;
  }

  // Create VASurface from the native pixmap.
  scoped_refptr<VASurface> va_surface =
      vaapi_wrapper_->CreateVASurfaceForVideoFrame(frame.get());
  if (!va_surface || va_surface->id() == VA_INVALID_ID) {
    VLOGF(1) << "Failed to create VASurface from VideoFrame";
    SetState(State::kError);
    return nullptr;
  }

  // Store the mapping between surface and video frame, so we know which video
  // frame to output when the surface is ready. It's also important to keep a
  // reference to the video frame during decoding, as the frame will be
  // automatically returned to the pool when the last reference is dropped.
  VASurfaceID surface_id = va_surface->id();
  DCHECK_EQ(output_frames_.count(surface_id), 0u);
  output_frames_[surface_id] = frame;

  // When the decoder is done using the frame for output or reference, it will
  // drop its reference to the surface. We can then safely destroy the surface
  // and remove the associated video frame from |output_frames_|. To be notified
  // when this happens we wrap the surface in another surface that calls
  // ReleaseFrameTask() on destruction. The |va_surface| object is bound to the
  // destruction callback to keep it alive, since the associated VAAPI surface
  // will be automatically destroyed when we drop the reference.
  VASurface::ReleaseCB release_frame_cb = base::BindOnce(
      &VaapiVideoDecoder::ReleaseFrameTask, weak_this_, std::move(va_surface));

  return new VASurface(surface_id, frame->layout().coded_size(),
                       GetVaFormatForVideoCodecProfile(profile_),
                       std::move(release_frame_cb));
}

void VaapiVideoDecoder::SurfaceReady(const scoped_refptr<VASurface>& va_surface,
                                     int32_t buffer_id,
                                     const gfx::Rect& visible_rect,
                                     const VideoColorSpace& /*color_space*/) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DCHECK_EQ(state_, State::kDecoding);
  DVLOGF(3);

  // Find the timestamp associated with |buffer_id|. It's possible that a
  // surface is output multiple times for different |buffer_id|s (e.g. VP9
  // show_existing_frame feature). This means we need to output the same frame
  // again with a different timestamp.
  // On some rare occasions it's also possible that a single DecoderBuffer
  // produces multiple surfaces with the same |buffer_id|, so we shouldn't
  // remove the timestamp from the cache.
  const auto it = buffer_id_to_timestamp_.Peek(buffer_id);
  DCHECK(it != buffer_id_to_timestamp_.end());
  base::TimeDelta timestamp = it->second;

  // Find the frame associated with the surface. We won't erase it from
  // |output_frames_| yet, as the decoder might still be using it for reference.
  DCHECK_EQ(output_frames_.count(va_surface->id()), 1u);
  OutputFrameTask(output_frames_[va_surface->id()], visible_rect, timestamp);
}

void VaapiVideoDecoder::OutputFrameTask(scoped_refptr<VideoFrame> video_frame,
                                        const gfx::Rect& visible_rect,
                                        base::TimeDelta timestamp) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DCHECK_EQ(state_, State::kDecoding);
  DCHECK(video_frame);
  DVLOGF(4);

  // Set the timestamp at which the decode operation started on the
  // |video_frame|. If the frame has been outputted before (e.g. because of VP9
  // show-existing-frame feature) we can't overwrite the timestamp directly, as
  // the original frame might still be in use. Instead we wrap the frame in
  // another frame with a different timestamp.
  if (video_frame->timestamp().is_zero())
    video_frame->set_timestamp(timestamp);

  if (video_frame->visible_rect() != visible_rect ||
      video_frame->timestamp() != timestamp) {
    gfx::Size natural_size = GetNaturalSize(visible_rect, pixel_aspect_ratio_);
    scoped_refptr<VideoFrame> wrapped_frame = VideoFrame::WrapVideoFrame(
        video_frame, video_frame->format(), visible_rect, natural_size);
    wrapped_frame->set_timestamp(timestamp);

    video_frame = std::move(wrapped_frame);
  }

  output_cb_.Run(std::move(video_frame));
}

void VaapiVideoDecoder::ChangeFrameResolutionTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DCHECK(state_ == State::kDecoding);
  DCHECK(output_frames_.empty());
  VLOGF(2);

  // TODO(hiroh): Handle profile changes.
  const gfx::Rect visible_rect = decoder_->GetVisibleRect();
  gfx::Size natural_size = GetNaturalSize(visible_rect, pixel_aspect_ratio_);
  gfx::Size pic_size = decoder_->GetPicSize();
  const base::Optional<VideoPixelFormat> format =
      GfxBufferFormatToVideoPixelFormat(GetBufferFormat());
  CHECK(format);
  frame_pool_->RequestFrames(Fourcc::FromVideoPixelFormat(*format), pic_size,
                             visible_rect, natural_size,
                             decoder_->GetRequiredNumOfPictures());

  // All pending decode operations will be completed before triggering a
  // resolution change, so we can safely destroy the context here.
  vaapi_wrapper_->DestroyContext();
  vaapi_wrapper_->CreateContext(pic_size);

  // Retry the current decode task.
  decoder_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VaapiVideoDecoder::HandleDecodeTask, weak_this_));
}

void VaapiVideoDecoder::ReleaseFrameTask(scoped_refptr<VASurface> va_surface,
                                         VASurfaceID surface_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DCHECK_EQ(va_surface->id(), surface_id);
  DVLOGF(4);

  // The decoder has finished using the frame associated with |surface_id| for
  // output or reference, so it's safe to drop our reference here. Once the
  // client drops its reference the frame will be automatically returned to the
  // pool for reuse.
  size_t num_erased = output_frames_.erase(surface_id);
  DCHECK_EQ(num_erased, 1u);
}

void VaapiVideoDecoder::NotifyFrameAvailableTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(4);

  // If we were waiting for output buffers, retry the current decode task.
  if (state_ == State::kWaitingForOutput) {
    DCHECK(current_decode_task_);
    SetState(State::kDecoding);
    decoder_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&VaapiVideoDecoder::HandleDecodeTask, weak_this_));
  }
}

void VaapiVideoDecoder::FlushTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DCHECK_EQ(state_, State::kDecoding);
  DCHECK(current_decode_task_);
  DCHECK(current_decode_task_->buffer_->end_of_stream());
  DCHECK(decode_task_queue_.empty());
  DVLOGF(2);

  // Flush will block until SurfaceReady() has been called for every frame
  // currently decoding.
  if (!decoder_->Flush()) {
    VLOGF(1) << "Failed to flush the decoder";
    SetState(State::kError);
    return;
  }

  // Put the decoder in an idle state, ready to resume. This will release all
  // VASurfaces currently held, so |output_frames_| should be empty after reset.
  decoder_->Reset();
  DCHECK(output_frames_.empty());

  // Notify the client flushing is done.
  std::move(current_decode_task_->decode_done_cb_).Run(DecodeStatus::OK);
  current_decode_task_ = base::nullopt;

  // Wait for new decodes, no decode tasks should be queued while flushing.
  SetState(State::kWaitingForInput);
}

void VaapiVideoDecoder::Reset(base::OnceClosure reset_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(2);

  // If we encountered an error, skip reset and notify client.
  if (state_ == State::kError) {
    std::move(reset_cb).Run();
    return;
  }

  // Put the decoder in an idle state, ready to resume. This will release all
  // VASurfaces currently held, so |output_frames_| should be empty after reset.
  decoder_->Reset();
  DCHECK(output_frames_.empty());
  SetState(State::kResetting);

  // Wait until any pending decode task has been aborted.
  decoder_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VaapiVideoDecoder::ResetDoneTask, weak_this_,
                                std::move(reset_cb)));
}

void VaapiVideoDecoder::ResetDoneTask(base::OnceClosure reset_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DCHECK_EQ(state_, State::kResetting);
  DCHECK(!current_decode_task_);
  DCHECK(decode_task_queue_.empty());
  DVLOGF(2);

  std::move(reset_cb).Run();
  SetState(State::kWaitingForInput);
}

void VaapiVideoDecoder::SetState(State state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(4) << static_cast<int>(state)
            << ", current state: " << static_cast<int>(state_);

  // Check whether the state change is valid.
  switch (state) {
    case State::kUninitialized:
      DCHECK_EQ(state_, State::kWaitingForInput);
      break;
    case State::kWaitingForInput:
      DCHECK(decode_task_queue_.empty());
      DCHECK(!current_decode_task_);
      DCHECK(state_ == State::kUninitialized || state_ == State::kDecoding ||
             state_ == State::kResetting);
      break;
    case State::kWaitingForOutput:
      DCHECK(current_decode_task_);
      DCHECK_EQ(state_, State::kDecoding);
      break;
    case State::kDecoding:
      DCHECK(state_ == State::kWaitingForInput ||
             state_ == State::kWaitingForOutput);
      break;
    case State::kResetting:
      DCHECK(state_ == State::kWaitingForInput ||
             state_ == State::kWaitingForOutput || state_ == State::kDecoding);
      ClearDecodeTaskQueue(DecodeStatus::ABORTED);
      break;
    case State::kError:
      ClearDecodeTaskQueue(DecodeStatus::DECODE_ERROR);
      break;
    default:
      NOTREACHED() << "Invalid state change";
  }

  state_ = state;
}

}  // namespace media

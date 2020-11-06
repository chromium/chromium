// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_video_decoder.h"

#include <limits>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/format_utils.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/gpu/chromeos/dmabuf_video_frame_pool.h"
#include "media/gpu/chromeos/platform_video_frame_utils.h"
#include "media/gpu/gpu_video_decode_accelerator_helpers.h"
#include "media/gpu/macros.h"
#include "media/gpu/vaapi/h264_vaapi_video_decoder_delegate.h"
#include "media/gpu/vaapi/va_surface.h"
#include "media/gpu/vaapi/vaapi_utils.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "media/gpu/vaapi/vp8_vaapi_video_decoder_delegate.h"
#include "media/gpu/vaapi/vp9_vaapi_video_decoder_delegate.h"

namespace media {

namespace {

// Size of the timestamp cache, needs to be large enough for frame-reordering.
constexpr size_t kTimestampCacheSize = 128;

// Returns the preferred VA_RT_FORMAT for the given |profile|.
unsigned int GetVaFormatForVideoCodecProfile(VideoCodecProfile profile) {
  if (profile == VP9PROFILE_PROFILE2 || profile == VP9PROFILE_PROFILE3)
    return VA_RT_FORMAT_YUV420_10BPP;
  return VA_RT_FORMAT_YUV420;
}

gfx::BufferFormat GetBufferFormat(VideoCodecProfile profile) {
#if defined(USE_OZONE)
  if (profile == VP9PROFILE_PROFILE2 || profile == VP9PROFILE_PROFILE3)
    return gfx::BufferFormat::P010;
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
std::unique_ptr<DecoderInterface> VaapiVideoDecoder::Create(
    scoped_refptr<base::SequencedTaskRunner> decoder_task_runner,
    base::WeakPtr<DecoderInterface::Client> client) {
  return base::WrapUnique<DecoderInterface>(
      new VaapiVideoDecoder(std::move(decoder_task_runner), std::move(client)));
}

// static
SupportedVideoDecoderConfigs VaapiVideoDecoder::GetSupportedConfigs(
    const gpu::GpuDriverBugWorkarounds& workarounds) {
  return ConvertFromSupportedProfiles(
      VaapiWrapper::GetSupportedDecodeProfiles(workarounds), false);
}

VaapiVideoDecoder::VaapiVideoDecoder(
    scoped_refptr<base::SequencedTaskRunner> decoder_task_runner,
    base::WeakPtr<DecoderInterface::Client> client)
    : DecoderInterface(std::move(decoder_task_runner), std::move(client)),
      buffer_id_to_timestamp_(kTimestampCacheSize),
      weak_this_factory_(this) {
  VLOGF(2);
  DCHECK(decoder_task_runner_->RunsTasksInCurrentSequence());
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  weak_this_ = weak_this_factory_.GetWeakPtr();
}

VaapiVideoDecoder::~VaapiVideoDecoder() {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Abort all currently scheduled decode tasks.
  ClearDecodeTaskQueue(DecodeStatus::ABORTED);

  weak_this_factory_.InvalidateWeakPtrs();

  // Notify |decoder_delegate_| of an imminent VAContextID destruction, so it
  // can destroy any internal structures making use of it.
  if (decoder_delegate_)
    decoder_delegate_->OnVAContextDestructionSoon();

  // Destroy explicitly to DCHECK() that |vaapi_wrapper_| references are held
  // inside the accelerator in |decoder_|, by the |allocated_va_surfaces_| and
  // of course by this class. To clear |allocated_va_surfaces_| we have to first
  // DestroyContext().
  decoder_ = nullptr;
  if (vaapi_wrapper_) {
    vaapi_wrapper_->DestroyContext();
    allocated_va_surfaces_.clear();

    DCHECK(vaapi_wrapper_->HasOneRef());
    vaapi_wrapper_ = nullptr;
  }
}

void VaapiVideoDecoder::Initialize(const VideoDecoderConfig& config,
                                   InitCB init_cb,
                                   const OutputCB& output_cb) {
  DVLOGF(2) << config.AsHumanReadableString();
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(config.IsValidConfig());
  DCHECK(state_ == State::kUninitialized || state_ == State::kWaitingForInput);

  // Reinitializing the decoder is allowed if there are no pending decodes.
  if (current_decode_task_ || !decode_task_queue_.empty()) {
    LOG(ERROR)
        << "Don't call Initialize() while there are pending decode tasks";
    std::move(init_cb).Run(StatusCode::kVaapiReinitializedDuringDecode);
    return;
  }

  // We expect the decoder to have released all output buffers (by the client
  // triggering a flush or reset), even if the
  // DecoderInterface API doesn't explicitly specify this.
  DCHECK(output_frames_.empty());

  if (state_ != State::kUninitialized) {
    DVLOGF(3) << "Reinitializing decoder";

    // Notify |decoder_delegate_| of an imminent VAContextID destruction, so it
    // can destroy any internal structures making use of it.
    decoder_delegate_->OnVAContextDestructionSoon();

    decoder_ = nullptr;
    DCHECK(vaapi_wrapper_);
    // To clear |allocated_va_surfaces_| we have to first DestroyContext().
    vaapi_wrapper_->DestroyContext();
    allocated_va_surfaces_.clear();

    DCHECK(vaapi_wrapper_->HasOneRef());
    vaapi_wrapper_ = nullptr;
    decoder_delegate_ = nullptr;
    SetState(State::kUninitialized);
  }

  // Initialize VAAPI wrapper.
  const VideoCodecProfile profile = config.profile();
  vaapi_wrapper_ = VaapiWrapper::CreateForVideoCodec(
      VaapiWrapper::kDecode, profile,
      base::Bind(&ReportVaapiErrorToUMA, "Media.VaapiVideoDecoder.VAAPIError"));
  UMA_HISTOGRAM_BOOLEAN("Media.VaapiVideoDecoder.VaapiWrapperCreationSuccess",
                        vaapi_wrapper_.get());
  if (!vaapi_wrapper_.get()) {
    VLOGF(1) << "Failed initializing VAAPI for profile "
             << GetProfileName(profile);
    std::move(init_cb).Run(StatusCode::kDecoderUnsupportedProfile);
    return;
  }

  profile_ = profile;
  color_space_ = config.color_space_info();
  auto accel_status = CreateAcceleratedVideoDecoder();
  if (!accel_status.is_ok()) {
    std::move(init_cb).Run(std::move(accel_status));
    return;
  }

  // Get and initialize the frame pool.
  DCHECK(client_);
  frame_pool_ = client_->GetVideoFramePool();

  pixel_aspect_ratio_ = config.GetPixelAspectRatio();

  output_cb_ = std::move(output_cb);
  SetState(State::kWaitingForInput);

  // Notify client initialization was successful.
  std::move(init_cb).Run(OkStatus());
}

void VaapiVideoDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                               DecodeCB decode_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (state_ == State::kError || state_ == State::kResetting)
    return;

  DCHECK_EQ(state_, State::kDecoding);
  DCHECK(current_decode_task_);

  // Check whether a flush was requested.
  if (current_decode_task_->buffer_->end_of_stream()) {
    Flush();
    return;
  }

  TRACE_EVENT_BEGIN0("media,gpu", "VaapiVideoDecoder::Decode");
  AcceleratedVideoDecoder::DecodeResult decode_result = decoder_->Decode();
  TRACE_EVENT_END0("media,gpu", "VaapiVideoDecoder::Decode");
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
    case AcceleratedVideoDecoder::kConfigChange:
      // A new set of output buffers is requested. We either didn't have any
      // output buffers yet or encountered a resolution change.
      // After the pipeline flushes all frames, ApplyResolutionChange() will be
      // called and we can start changing resolution.
      DCHECK(client_);
      SetState(State::kChangingResolution);
      client_->PrepareChangeResolution();
      break;
    case AcceleratedVideoDecoder::kRanOutOfSurfaces:
      // No more surfaces to decode into available, wait until client returns
      // video frames to the frame pool.
      SetState(State::kWaitingForOutput);
      break;
    case AcceleratedVideoDecoder::kNeedContextUpdate:
      LOG(ERROR) << "Context updates not supported";
      SetState(State::kError);
      break;
    case AcceleratedVideoDecoder::kDecodeError:
      LOG(ERROR) << "Error decoding stream";
      UMA_HISTOGRAM_BOOLEAN("Media.VaapiVideoDecoder.DecodeError", true);
      SetState(State::kError);
      break;
    case AcceleratedVideoDecoder::kTryAgain:
      LOG(ERROR) << "Encrypted streams not supported";
      SetState(State::kError);
      break;
  }
}

void VaapiVideoDecoder::ClearDecodeTaskQueue(DecodeStatus status) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

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
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, State::kDecoding);
  DCHECK(current_decode_task_);

  // Get a video frame from the video frame pool.
  scoped_refptr<VideoFrame> frame = frame_pool_->GetFrame();
  if (!frame) {
    // Ask the video frame pool to notify us when new frames are available, so
    // we can retry the current decode task.
    frame_pool_->NotifyWhenFrameAvailable(
        base::BindOnce(&VaapiVideoDecoder::NotifyFrameAvailable, weak_this_));
    return nullptr;
  }

  // |frame|s coming from ARC++ are not GpuMemoryBuffer-backed, but they have
  // DmaBufs whose fd numbers are consistent along the lifetime of the VA
  // surfaces they back.
  DCHECK(frame->GetGpuMemoryBuffer() || frame->HasDmaBufs());
  const gfx::GpuMemoryBufferId frame_id =
      frame->GetGpuMemoryBuffer()
          ? frame->GetGpuMemoryBuffer()->GetId()
          : gfx::GpuMemoryBufferId(frame->DmabufFds()[0].get());

  scoped_refptr<VASurface> va_surface;
  if (!base::Contains(allocated_va_surfaces_, frame_id)) {
    scoped_refptr<gfx::NativePixmap> pixmap =
        CreateNativePixmapDmaBuf(frame.get());
    if (!pixmap) {
      LOG(ERROR) << "Failed to create NativePixmap from VideoFrame";
      SetState(State::kError);
      return nullptr;
    }

    va_surface = vaapi_wrapper_->CreateVASurfaceForPixmap(std::move(pixmap));
    if (!va_surface || va_surface->id() == VA_INVALID_ID) {
      LOG(ERROR) << "Failed to create VASurface from VideoFrame";
      SetState(State::kError);
      return nullptr;
    }

    allocated_va_surfaces_[frame_id] = va_surface;
  } else {
    va_surface = allocated_va_surfaces_[frame_id];
    DCHECK_EQ(frame->coded_size(), va_surface->size());
  }

  // Store the mapping between surface and video frame, so we know which video
  // frame to output when the surface is ready. It's also important to keep a
  // reference to the video frame during decoding, as the frame will be
  // automatically returned to the pool when the last reference is dropped.
  VASurfaceID surface_id = va_surface->id();
  DCHECK_EQ(output_frames_.count(surface_id), 0u);
  output_frames_[surface_id] = frame;

  // When the decoder is done using the frame for output or reference, it will
  // drop its reference to the surface. We can then safely remove the associated
  // video frame from |output_frames_|. To be notified when this happens we wrap
  // the surface in another surface with ReleaseVideoFrame() as destruction
  // observer.
  VASurface::ReleaseCB release_frame_cb =
      base::BindOnce(&VaapiVideoDecoder::ReleaseVideoFrame, weak_this_);

  return new VASurface(surface_id, frame->layout().coded_size(),
                       GetVaFormatForVideoCodecProfile(profile_),
                       std::move(release_frame_cb));
}

void VaapiVideoDecoder::SurfaceReady(scoped_refptr<VASurface> va_surface,
                                     int32_t buffer_id,
                                     const gfx::Rect& visible_rect,
                                     const VideoColorSpace& color_space) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, State::kDecoding);

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
  scoped_refptr<VideoFrame> video_frame = output_frames_[va_surface->id()];

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

  const auto gfx_color_space = color_space.ToGfxColorSpace();
  if (gfx_color_space.IsValid())
    video_frame->set_color_space(gfx_color_space);

  output_cb_.Run(std::move(video_frame));
}

void VaapiVideoDecoder::ApplyResolutionChange() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(state_ == State::kChangingResolution ||
         state_ == State::kWaitingForInput);
  DCHECK(output_frames_.empty());
  VLOGF(2);

  const gfx::Rect visible_rect = decoder_->GetVisibleRect();
  const gfx::Size natural_size =
      GetNaturalSize(visible_rect, pixel_aspect_ratio_);
  const gfx::Size pic_size = decoder_->GetPicSize();
  const base::Optional<VideoPixelFormat> format =
      GfxBufferFormatToVideoPixelFormat(
          GetBufferFormat(decoder_->GetProfile()));
  CHECK(format);
  auto format_fourcc = Fourcc::FromVideoPixelFormat(*format);
  CHECK(format_fourcc);
  if (!frame_pool_->Initialize(*format_fourcc, pic_size, visible_rect,
                               natural_size,
                               decoder_->GetRequiredNumOfPictures())) {
    DLOG(WARNING) << "Failed Initialize()ing the frame pool.";
    SetState(State::kError);
    return;
  }

  // Notify |decoder_delegate_| of an imminent VAContextID destruction, so it
  // can destroy any internal structures making use of it.
  decoder_delegate_->OnVAContextDestructionSoon();

  // All pending decode operations will be completed before triggering a
  // resolution change, so we can safely DestroyContext() here; that, in turn,
  // allows for clearing the |allocated_va_surfaces_|.
  vaapi_wrapper_->DestroyContext();
  allocated_va_surfaces_.clear();

  if (profile_ != decoder_->GetProfile()) {
    // When a profile is changed, we need to re-initialize VaapiWrapper.
    profile_ = decoder_->GetProfile();
    auto new_vaapi_wrapper = VaapiWrapper::CreateForVideoCodec(
        VaapiWrapper::kDecode, profile_,
        base::Bind(&ReportVaapiErrorToUMA,
                   "Media.VaapiVideoDecoder.VAAPIError"));
    if (!new_vaapi_wrapper.get()) {
      DLOG(WARNING) << "Failed creating VaapiWrapper";
      SetState(State::kError);
      return;
    }
    decoder_delegate_->set_vaapi_wrapper(new_vaapi_wrapper.get());
    vaapi_wrapper_ = std::move(new_vaapi_wrapper);
  }

  if (!vaapi_wrapper_->CreateContext(pic_size)) {
    VLOGF(1) << "Failed creating context";
    SetState(State::kError);
    return;
  }

  // If we reset during resolution change, then there is no decode tasks. In
  // this case we do nothing and wait for next input. Otherwise, continue
  // decoding the current task.
  if (current_decode_task_) {
    // Retry the current decode task.
    SetState(State::kDecoding);
    decoder_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&VaapiVideoDecoder::HandleDecodeTask, weak_this_));
  }
}

void VaapiVideoDecoder::ReleaseVideoFrame(VASurfaceID surface_id) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The decoder has finished using the frame associated with |surface_id| for
  // output or reference, so it's safe to drop our reference here. Once the
  // client drops its reference the frame will be automatically returned to the
  // pool for reuse.
  size_t num_erased = output_frames_.erase(surface_id);
  DCHECK_EQ(num_erased, 1u);
}

void VaapiVideoDecoder::NotifyFrameAvailable() {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If we were waiting for output buffers, retry the current decode task.
  if (state_ == State::kWaitingForOutput) {
    DCHECK(current_decode_task_);
    SetState(State::kDecoding);
    decoder_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&VaapiVideoDecoder::HandleDecodeTask, weak_this_));
  }
}

void VaapiVideoDecoder::Flush() {
  DVLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, State::kDecoding);
  DCHECK(current_decode_task_);
  DCHECK(current_decode_task_->buffer_->end_of_stream());
  DCHECK(decode_task_queue_.empty());

  // Flush will block until SurfaceReady() has been called for every frame
  // currently decoding.
  if (!decoder_->Flush()) {
    LOG(ERROR) << "Failed to flush the decoder";
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
  DVLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If we encountered an error, skip reset and notify client.
  if (state_ == State::kError) {
    std::move(reset_cb).Run();
    return;
  }

  if (state_ == State::kChangingResolution) {
    // Recreate |decoder_| and |decoder_delegate_| if we are Reset() in the
    // interim between calling |client_|s PrepareChangeResolution() and being
    // called back on ApplyResolutionChange(), so the latter will find a fresh
    // |decoder_|. Also give a chance to |decoder_delegate_| to release its
    // internal data structures.
    decoder_delegate_->OnVAContextDestructionSoon();
    if (!CreateAcceleratedVideoDecoder().is_ok()) {
      SetState(State::kError);
      std::move(reset_cb).Run();
      return;
    }
  } else {
    // Put the decoder in an idle state, ready to resume. This will release all
    // VASurfaces currently held, so |output_frames_| should be empty after
    // reset.
    decoder_->Reset();
  }

  DCHECK(output_frames_.empty());
  SetState(State::kResetting);

  // Wait until any pending decode task has been aborted.
  decoder_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VaapiVideoDecoder::ResetDone, weak_this_,
                                std::move(reset_cb)));
}

Status VaapiVideoDecoder::CreateAcceleratedVideoDecoder() {
  DVLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (profile_ >= H264PROFILE_MIN && profile_ <= H264PROFILE_MAX) {
    auto accelerator =
        std::make_unique<H264VaapiVideoDecoderDelegate>(this, vaapi_wrapper_);
    decoder_delegate_ = accelerator.get();

    decoder_.reset(
        new H264Decoder(std::move(accelerator), profile_, color_space_));
  } else if (profile_ >= VP8PROFILE_MIN && profile_ <= VP8PROFILE_MAX) {
    auto accelerator =
        std::make_unique<VP8VaapiVideoDecoderDelegate>(this, vaapi_wrapper_);
    decoder_delegate_ = accelerator.get();

    decoder_.reset(new VP8Decoder(std::move(accelerator)));
  } else if (profile_ >= VP9PROFILE_MIN && profile_ <= VP9PROFILE_MAX) {
    auto accelerator =
        std::make_unique<VP9VaapiVideoDecoderDelegate>(this, vaapi_wrapper_);
    decoder_delegate_ = accelerator.get();

    decoder_.reset(
        new VP9Decoder(std::move(accelerator), profile_, color_space_));
  } else {
    return Status(StatusCode::kDecoderUnsupportedProfile)
        .WithData("profile", profile_);
  }
  return OkStatus();
}

void VaapiVideoDecoder::ResetDone(base::OnceClosure reset_cb) {
  DVLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, State::kResetting);
  DCHECK(!current_decode_task_);
  DCHECK(decode_task_queue_.empty());

  std::move(reset_cb).Run();
  SetState(State::kWaitingForInput);
}

void VaapiVideoDecoder::SetState(State state) {
  DVLOGF(3) << static_cast<int>(state)
            << ", current state: " << static_cast<int>(state_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

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
             state_ == State::kWaitingForOutput ||
             state_ == State::kChangingResolution);
      break;
    case State::kResetting:
      DCHECK(state_ == State::kWaitingForInput ||
             state_ == State::kWaitingForOutput || state_ == State::kDecoding);
      ClearDecodeTaskQueue(DecodeStatus::ABORTED);
      break;
    case State::kChangingResolution:
      DCHECK_EQ(state_, State::kDecoding);
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

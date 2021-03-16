// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_video_decoder.h"

#include <limits>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/containers/fixed_flat_map.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/trace_event/trace_event.h"
#include "build/chromeos_buildflags.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/format_utils.h"
#include "media/base/media_switches.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/gpu/av1_decoder.h"
#include "media/gpu/chromeos/dmabuf_video_frame_pool.h"
#include "media/gpu/chromeos/platform_video_frame_utils.h"
#include "media/gpu/gpu_video_decode_accelerator_helpers.h"
#include "media/gpu/macros.h"
#include "media/gpu/vaapi/av1_vaapi_video_decoder_delegate.h"
#include "media/gpu/vaapi/h264_vaapi_video_decoder_delegate.h"
#include "media/gpu/vaapi/va_surface.h"
#include "media/gpu/vaapi/vaapi_utils.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "media/gpu/vaapi/vp8_vaapi_video_decoder_delegate.h"
#include "media/gpu/vaapi/vp9_vaapi_video_decoder_delegate.h"
#include "media/media_buildflags.h"

#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
#include "media/gpu/vaapi/h265_vaapi_video_decoder_delegate.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/components/cdm_factory_daemon/chromeos_cdm_factory.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace media {

namespace {

// Size of the timestamp cache, needs to be large enough for frame-reordering.
constexpr size_t kTimestampCacheSize = 128;

base::Optional<VideoPixelFormat> GetPixelFormatForBitDepth(uint8_t bit_depth) {
  constexpr auto kSupportedBitDepthAndGfxFormats = base::MakeFixedFlatMap<
      uint8_t, gfx::BufferFormat>({
#if defined(USE_OZONE)
    {8u, gfx::BufferFormat::YUV_420_BIPLANAR}, {10u, gfx::BufferFormat::P010},
#else
    {8u, gfx::BufferFormat::RGBX_8888},
#endif  // defined(USE_OZONE)
  });
  if (!base::Contains(kSupportedBitDepthAndGfxFormats, bit_depth)) {
    VLOGF(1) << "Unsupported bit depth: " << base::strict_cast<int>(bit_depth);
    return base::nullopt;
  }
  return GfxBufferFormatToVideoPixelFormat(
      kSupportedBitDepthAndGfxFormats.at(bit_depth));
}

inline int RoundDownToEven(int x) {
  DCHECK_GE(x, 0);
  return x - (x % 2);
}

inline int RoundUpToEven(int x) {
  DCHECK_GE(x, 0);
  CHECK_LT(x, std::numeric_limits<int>::max());
  return x + (x % 2);
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
      VaapiWrapper::GetSupportedDecodeProfiles(workarounds),
#if BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)
      true /* allow_encrypted */);
#else
      false /* allow_encrypted */);
#endif
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
  // inside the accelerator in |decoder_|, by the |allocated_va_surfaces_|, by
  // the |decode_surface_pool_for_scaling_| and of course by this class. To
  // clear |allocated_va_surfaces_| and |decode_surface_pool_for_scaling_| we
  // have to first DestroyContext().
  decoder_ = nullptr;
  if (vaapi_wrapper_) {
    vaapi_wrapper_->DestroyContext();
    allocated_va_surfaces_.clear();
    while (!decode_surface_pool_for_scaling_.empty())
      decode_surface_pool_for_scaling_.pop();

    DCHECK(vaapi_wrapper_->HasOneRef());
    vaapi_wrapper_ = nullptr;
  }
}

void VaapiVideoDecoder::Initialize(const VideoDecoderConfig& config,
                                   CdmContext* cdm_context,
                                   InitCB init_cb,
                                   const OutputCB& output_cb,
                                   const WaitingCB& waiting_cb) {
  DVLOGF(2) << config.AsHumanReadableString();
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(config.IsValidConfig());
  DCHECK(state_ == State::kError || state_ == State::kUninitialized ||
         state_ == State::kWaitingForInput);

  // Reinitializing the decoder is allowed if there are no pending decodes.
  if (current_decode_task_ || !decode_task_queue_.empty()) {
    LOG(ERROR)
        << "Don't call Initialize() while there are pending decode tasks";
    std::move(init_cb).Run(StatusCode::kVaapiReinitializedDuringDecode);
    return;
  }

  if (state_ != State::kUninitialized) {
    DVLOGF(3) << "Reinitializing decoder";

    // Notify |decoder_delegate_| of an imminent VAContextID destruction, so it
    // can destroy any internal structures making use of it.
    decoder_delegate_->OnVAContextDestructionSoon();

    decoder_ = nullptr;
    DCHECK(vaapi_wrapper_);
    // To clear |allocated_va_surfaces_| and |decode_surface_pool_for_scaling_|
    // we have to first DestroyContext().
    vaapi_wrapper_->DestroyContext();
    allocated_va_surfaces_.clear();
    while (!decode_surface_pool_for_scaling_.empty())
      decode_surface_pool_for_scaling_.pop();
    decode_to_output_scale_factor_.reset();

    DCHECK(vaapi_wrapper_->HasOneRef());
    vaapi_wrapper_ = nullptr;
    decoder_delegate_ = nullptr;

#if BUILDFLAG(IS_CHROMEOS_ASH)
    // |cdm_context_ref_| is reset after |decoder_| because we passed
    // |cdm_context_ref_->GetCdmContext()| when creating the |decoder_|, so we
    // don't want |decoder_| to have a dangling pointer. We also destroy
    // |cdm_event_cb_registration_| before |cdm_context_ref_| so that we have a
    // CDM at the moment of destroying the callback registration.
    cdm_event_cb_registration_ = nullptr;
    cdm_context_ref_ = nullptr;
#endif

    SetState(State::kUninitialized);
  }
  DCHECK(!current_decode_task_);
  DCHECK(decode_task_queue_.empty());

  // Destroying the |decoder_| during re-initialization should release all
  // output buffers (and there should be no output buffers to begin with if the
  // decoder was previously uninitialized).
  DCHECK(output_frames_.empty());

  if (config.is_encrypted()) {
#if !BUILDFLAG(IS_CHROMEOS_ASH)
    SetState(State::kError);
    std::move(init_cb).Run(StatusCode::kEncryptedContentUnsupported);
    return;
#else
    if (!cdm_context || !cdm_context->GetChromeOsCdmContext()) {
      LOG(ERROR) << "Cannot support encrypted stream w/out ChromeOsCdmContext";
      SetState(State::kError);
      std::move(init_cb).Run(StatusCode::kDecoderMissingCdmForEncryptedContent);
      return;
    }
    if (config.codec() != kCodecH264 && config.codec() != kCodecVP9 &&
        config.codec() != kCodecHEVC) {
      VLOGF(1)
          << "Vaapi decoder does not support this codec for encrypted content";
      SetState(State::kError);
      std::move(init_cb).Run(StatusCode::kEncryptedContentUnsupported);
      return;
    }
    cdm_event_cb_registration_ = cdm_context->RegisterEventCB(
        base::BindRepeating(&VaapiVideoDecoder::OnCdmContextEvent,
                            weak_this_factory_.GetWeakPtr()));
    cdm_context_ref_ = cdm_context->GetChromeOsCdmContext()->GetCdmContextRef();
#endif
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
  } else if (config.codec() == kCodecHEVC &&
             !base::CommandLine::ForCurrentProcess()->HasSwitch(
                 switches::kEnableClearHevcForTesting)) {
    DVLOG(1) << "Clear HEVC content is not supported";
    SetState(State::kError);
    std::move(init_cb).Run(StatusCode::kClearContentUnsupported);
    return;
#endif
  }

  // Initialize VAAPI wrapper.
  const VideoCodecProfile profile = config.profile();
  vaapi_wrapper_ = VaapiWrapper::CreateForVideoCodec(
#if BUILDFLAG(IS_CHROMEOS_ASH)
      !cdm_context_ref_ ? VaapiWrapper::kDecode
                        : VaapiWrapper::kDecodeProtected,
#else
      VaapiWrapper::kDecode,
#endif
      profile, config.encryption_scheme(),
      base::BindRepeating(&ReportVaapiErrorToUMA,
                          "Media.VaapiVideoDecoder.VAAPIError"));
  UMA_HISTOGRAM_BOOLEAN("Media.VaapiVideoDecoder.VaapiWrapperCreationSuccess",
                        vaapi_wrapper_.get());
  if (!vaapi_wrapper_.get()) {
    VLOGF(1) << "Failed initializing VAAPI for profile "
             << GetProfileName(profile);
    SetState(State::kError);
    std::move(init_cb).Run(StatusCode::kDecoderUnsupportedProfile);
    return;
  }

  profile_ = profile;
  color_space_ = config.color_space_info();
  encryption_scheme_ = config.encryption_scheme();
  auto accel_status = CreateAcceleratedVideoDecoder();
  if (!accel_status.is_ok()) {
    SetState(State::kError);
    std::move(init_cb).Run(std::move(accel_status));
    return;
  }

  // Get and initialize the frame pool.
  DCHECK(client_);
  frame_pool_ = client_->GetVideoFramePool();

  pixel_aspect_ratio_ = config.GetPixelAspectRatio();

  output_cb_ = std::move(output_cb);
  waiting_cb_ = std::move(waiting_cb);
  SetState(State::kWaitingForInput);

  // Notify client initialization was successful.
  std::move(init_cb).Run(OkStatus());
}

void VaapiVideoDecoder::OnCdmContextEvent(CdmContext::Event event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (event != CdmContext::Event::kHasAdditionalUsableKey)
    return;

  // Invoke the callback we'd get for a protected session update because this is
  // the same thing, it's a trigger that there are new keys, so if we were
  // waiting for a key we should fetch them again.
  ProtectedSessionUpdate(true);
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
      DVLOG(1) << "Decoder going into the waiting for protected state";
      DCHECK_NE(encryption_scheme_, EncryptionScheme::kUnencrypted);
      SetState(State::kWaitingForProtected);
      // If we have lost our protected HW session, it should be recoverable, so
      // indicate that we have lost our decoder state so it can be reloaded.
      if (decoder_delegate_->HasInitiatedProtectedRecovery())
        waiting_cb_.Run(WaitingReason::kDecoderStateLost);
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
                       va_surface->format(), std::move(release_frame_cb));
}

scoped_refptr<VASurface> VaapiVideoDecoder::CreateDecodeSurface() {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, State::kDecoding);
  DCHECK(current_decode_task_);

  if (decode_surface_pool_for_scaling_.empty())
    return nullptr;

  // Get surface from pool.
  std::unique_ptr<ScopedVASurface> surface =
      std::move(decode_surface_pool_for_scaling_.front());
  decode_surface_pool_for_scaling_.pop();
  // Gather information about the surface to avoid use-after-move.
  const VASurfaceID surface_id = surface->id();
  const gfx::Size surface_size = surface->size();
  const unsigned int surface_format = surface->format();
  // Wrap the ScopedVASurface inside a VASurface indirectly.
  VASurface::ReleaseCB release_decode_surface_cb =
      base::BindOnce(&VaapiVideoDecoder::ReturnDecodeSurfaceToPool, weak_this_,
                     std::move(surface));
  return new VASurface(surface_id, surface_size, surface_format,
                       std::move(release_decode_surface_cb));
}

bool VaapiVideoDecoder::IsScalingDecode() {
  // If we're not decoding while scaling, we shouldn't have any surfaces for
  // that purpose.
  DCHECK(!!decode_to_output_scale_factor_ ||
         decode_surface_pool_for_scaling_.empty());
  return !!decode_to_output_scale_factor_;
}

const gfx::Rect VaapiVideoDecoder::GetOutputVisibleRect(
    const gfx::Rect& decode_visible_rect,
    const gfx::Size& output_picture_size) {
  if (!IsScalingDecode())
    return decode_visible_rect;
  DCHECK_LT(*decode_to_output_scale_factor_, 1.0f);
  gfx::Rect output_rect =
      ScaleToEnclosedRect(decode_visible_rect, *decode_to_output_scale_factor_);
  // Make the dimensions even numbered to align with other requirements later in
  // the pipeline.
  output_rect.set_width(RoundDownToEven(output_rect.width()));
  output_rect.set_height(RoundDownToEven(output_rect.height()));
  CHECK(gfx::Rect(output_picture_size).Contains(output_rect));
  return output_rect;
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

  if (cdm_context_ref_) {
    // For protected content we also need to set the ID for validating protected
    // surfaces in the VideoFrame metadata so we can check if the surface is
    // still valid once we get to the compositor stage.
    uint32_t protected_instance_id = vaapi_wrapper_->GetProtectedInstanceID();
    video_frame->metadata().hw_protected_validation_id = protected_instance_id;
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

  if (cdm_context_ref_) {
    // Get the screen resolutions so we can determine if we should pre-scale
    // content during decoding to maximize use of overlay downscaling since
    // protected content requires overlays currently.
    // NOTE: Only use this for protected content as other requirements for using
    // it are tied to protected content.
#if BUILDFLAG(IS_CHROMEOS_ASH)
    chromeos::ChromeOsCdmFactory::GetScreenResolutions(BindToCurrentLoop(
        base::BindOnce(&VaapiVideoDecoder::ApplyResolutionChangeWithScreenSizes,
                       weak_this_)));
    return;
#endif
  }
  ApplyResolutionChangeWithScreenSizes(std::vector<gfx::Size>());
}

void VaapiVideoDecoder::ApplyResolutionChangeWithScreenSizes(
    const std::vector<gfx::Size>& screen_resolutions) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(state_ == State::kChangingResolution ||
         state_ == State::kWaitingForInput || state_ == State::kResetting ||
         state_ == State::kError);
  DCHECK(output_frames_.empty());
  VLOGF(2);
  // If we are not in the state for changing resolution, then skip doing it. For
  // all the other states, those can occur because something happened after the
  // async call to get the screen sizes in ApplyResolutionChange(), and in that
  // case we will get another resolution change event when the decoder parses
  // the resolution and notifies us.
  if (state_ != State::kChangingResolution)
    return;

  const uint8_t bit_depth = decoder_->GetBitDepth();
  const base::Optional<VideoPixelFormat> format =
      GetPixelFormatForBitDepth(bit_depth);
  if (!format) {
    SetState(State::kError);
    return;
  }

  // Notify |decoder_delegate_| of an imminent VAContextID destruction, so it
  // can destroy any internal structures making use of it.
  decoder_delegate_->OnVAContextDestructionSoon();

  // All pending decode operations will be completed before triggering a
  // resolution change, so we can safely DestroyContext() here; that, in turn,
  // allows for clearing the |allocated_va_surfaces_| and the
  // |decode_surface_pool_for_scaling_|.
  vaapi_wrapper_->DestroyContext();
  allocated_va_surfaces_.clear();

  while (!decode_surface_pool_for_scaling_.empty())
    decode_surface_pool_for_scaling_.pop();
  decode_to_output_scale_factor_.reset();

  gfx::Rect output_visible_rect = decoder_->GetVisibleRect();
  gfx::Size output_pic_size = decoder_->GetPicSize();
  if (output_pic_size.IsEmpty()) {
    DLOG(ERROR) << "Empty picture size in decoder";
    SetState(State::kError);
    return;
  }
  const auto format_fourcc = Fourcc::FromVideoPixelFormat(*format);
  CHECK(format_fourcc);
  if (!screen_resolutions.empty()) {
    // Ideally we would base this off visible size, but that can change
    // midstream without forcing a config change, so we need to scale the
    // overall decoded image and then apply that same relative scaling to the
    // visible rect later.
    CHECK(cdm_context_ref_);
    gfx::Size max_desired_size;
    const float pic_aspect =
        static_cast<float>(output_pic_size.width()) / output_pic_size.height();
    for (const auto& screen : screen_resolutions) {
      if (screen.IsEmpty())
        continue;
      int target_width;
      int target_height;
      const float screen_aspect =
          static_cast<float>(screen.width()) / screen.height();
      if (pic_aspect >= screen_aspect) {
        // Constrain on width.
        if (screen.width() < output_pic_size.width()) {
          target_width = screen.width();
          target_height =
              base::checked_cast<int>(std::lround(target_width / pic_aspect));
        } else {
          target_width = output_pic_size.width();
          target_height = output_pic_size.height();
        }
      } else {
        // Constrain on height.
        if (screen.height() < output_pic_size.height()) {
          target_height = screen.height();
          target_width =
              base::checked_cast<int>(std::lround(target_height * pic_aspect));
        } else {
          target_height = output_pic_size.height();
          target_width = output_pic_size.width();
        }
      }
      if (target_width > max_desired_size.width() ||
          target_height > max_desired_size.height()) {
        max_desired_size.SetSize(target_width, target_height);
      }
    }
    if (!max_desired_size.IsEmpty() &&
        max_desired_size.width() < output_pic_size.width()) {
      // Fix this so we are sure it's on a multiple of two to deal with
      // subsampling.
      max_desired_size.set_width(RoundUpToEven(max_desired_size.width()));
      max_desired_size.set_height(RoundUpToEven(max_desired_size.height()));
      decode_to_output_scale_factor_ =
          static_cast<float>(max_desired_size.width()) /
          output_pic_size.width();
      output_pic_size = max_desired_size;
      output_visible_rect =
          GetOutputVisibleRect(output_visible_rect, output_pic_size);

      // Create the surface pool for decoding, the normal pool will be used for
      // output.
      const size_t decode_pool_size = decoder_->GetRequiredNumOfPictures();
      const base::Optional<gfx::BufferFormat> buffer_format =
          VideoPixelFormatToGfxBufferFormat(*format);
      if (!buffer_format) {
        decode_to_output_scale_factor_.reset();
        SetState(State::kError);
        return;
      }
      const uint32_t va_fourcc =
          VaapiWrapper::BufferFormatToVAFourCC(*buffer_format);
      const uint32_t va_rt_format =
          VaapiWrapper::BufferFormatToVARTFormat(*buffer_format);
      if (!va_fourcc || !va_rt_format) {
        decode_to_output_scale_factor_.reset();
        SetState(State::kError);
        return;
      }
      const gfx::Size decoder_pic_size = decoder_->GetPicSize();
      for (size_t i = 0; i < decode_pool_size; ++i) {
        std::unique_ptr<ScopedVASurface> surface =
            vaapi_wrapper_->CreateScopedVASurface(
                base::strict_cast<unsigned int>(va_rt_format), decoder_pic_size,
                /*visible_size=*/base::nullopt, va_fourcc);
        if (!surface) {
          while (!decode_surface_pool_for_scaling_.empty())
            decode_surface_pool_for_scaling_.pop();
          decode_to_output_scale_factor_.reset();
          SetState(State::kError);
          return;
        }
        decode_surface_pool_for_scaling_.push(std::move(surface));
      }
    }
  }
  const gfx::Size natural_size =
      GetNaturalSize(output_visible_rect, pixel_aspect_ratio_);
  if (!frame_pool_->Initialize(
          *format_fourcc, output_pic_size, output_visible_rect, natural_size,
          decoder_->GetRequiredNumOfPictures(), !!cdm_context_ref_)) {
    DLOG(WARNING) << "Failed Initialize()ing the frame pool.";
    SetState(State::kError);
    return;
  }

  if (profile_ != decoder_->GetProfile()) {
    // When a profile is changed, we need to re-initialize VaapiWrapper.
    profile_ = decoder_->GetProfile();
    auto new_vaapi_wrapper = VaapiWrapper::CreateForVideoCodec(
#if BUILDFLAG(IS_CHROMEOS_ASH)
        !cdm_context_ref_ ? VaapiWrapper::kDecode
                          : VaapiWrapper::kDecodeProtected,
#else
        VaapiWrapper::kDecode,
#endif
        profile_, encryption_scheme_,
        base::BindRepeating(&ReportVaapiErrorToUMA,
                            "Media.VaapiVideoDecoder.VAAPIError"));
    if (!new_vaapi_wrapper.get()) {
      DLOG(WARNING) << "Failed creating VaapiWrapper";
      SetState(State::kError);
      return;
    }
    decoder_delegate_->set_vaapi_wrapper(new_vaapi_wrapper.get());
    vaapi_wrapper_ = std::move(new_vaapi_wrapper);
  }

  if (!vaapi_wrapper_->CreateContext(decoder_->GetPicSize())) {
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

void VaapiVideoDecoder::ProtectedSessionUpdate(bool success) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!success) {
    LOG(ERROR) << "Terminating decoding after failed protected update";
    SetState(State::kError);
    return;
  }

  // If we were waiting for a protected update, retry the current decode task.
  if (state_ != State::kWaitingForProtected)
    return;

  DCHECK(current_decode_task_);
  SetState(State::kDecoding);
  decoder_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VaapiVideoDecoder::HandleDecodeTask, weak_this_));
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

  VaapiVideoDecoderDelegate::ProtectedSessionUpdateCB protected_update_cb =
      BindToCurrentLoop(base::BindRepeating(
          &VaapiVideoDecoder::ProtectedSessionUpdate, weak_this_));
  if (profile_ >= H264PROFILE_MIN && profile_ <= H264PROFILE_MAX) {
    auto accelerator = std::make_unique<H264VaapiVideoDecoderDelegate>(
        this, vaapi_wrapper_, std::move(protected_update_cb),
        cdm_context_ref_ ? cdm_context_ref_->GetCdmContext() : nullptr,
        encryption_scheme_);
    decoder_delegate_ = accelerator.get();

    decoder_.reset(
        new H264Decoder(std::move(accelerator), profile_, color_space_));
  } else if (profile_ >= VP8PROFILE_MIN && profile_ <= VP8PROFILE_MAX) {
    auto accelerator =
        std::make_unique<VP8VaapiVideoDecoderDelegate>(this, vaapi_wrapper_);
    decoder_delegate_ = accelerator.get();

    decoder_.reset(new VP8Decoder(std::move(accelerator)));
  } else if (profile_ >= VP9PROFILE_MIN && profile_ <= VP9PROFILE_MAX) {
    auto accelerator = std::make_unique<VP9VaapiVideoDecoderDelegate>(
        this, vaapi_wrapper_, std::move(protected_update_cb),
        cdm_context_ref_ ? cdm_context_ref_->GetCdmContext() : nullptr,
        encryption_scheme_);
    decoder_delegate_ = accelerator.get();

    decoder_.reset(
        new VP9Decoder(std::move(accelerator), profile_, color_space_));
  }
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
  else if (profile_ >= HEVCPROFILE_MIN && profile_ <= HEVCPROFILE_MAX) {
    auto accelerator = std::make_unique<H265VaapiVideoDecoderDelegate>(
        this, vaapi_wrapper_, std::move(protected_update_cb),
        cdm_context_ref_ ? cdm_context_ref_->GetCdmContext() : nullptr,
        encryption_scheme_);
    decoder_delegate_ = accelerator.get();

    decoder_.reset(
        new H265Decoder(std::move(accelerator), profile_, color_space_));
  }
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC)
  else if (profile_ >= AV1PROFILE_MIN && profile_ <= AV1PROFILE_MAX) {
    auto accelerator =
        std::make_unique<AV1VaapiVideoDecoderDelegate>(this, vaapi_wrapper_);
    decoder_delegate_ = accelerator.get();

    decoder_.reset(new AV1Decoder(std::move(accelerator), profile_));
  }
  else {
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
      DCHECK(state_ == State::kWaitingForInput || state_ == State::kError);
      break;
    case State::kWaitingForInput:
      DCHECK(decode_task_queue_.empty());
      DCHECK(!current_decode_task_);
      DCHECK(state_ == State::kUninitialized || state_ == State::kDecoding ||
             state_ == State::kResetting);
      break;
    case State::kWaitingForProtected:
      DCHECK(!!cdm_context_ref_);
      FALLTHROUGH;
    case State::kWaitingForOutput:
      DCHECK(current_decode_task_);
      DCHECK_EQ(state_, State::kDecoding);
      break;
    case State::kDecoding:
      DCHECK(state_ == State::kWaitingForInput ||
             state_ == State::kWaitingForOutput ||
             state_ == State::kChangingResolution ||
             state_ == State::kWaitingForProtected);
      break;
    case State::kResetting:
      DCHECK(state_ == State::kWaitingForInput ||
             state_ == State::kWaitingForOutput || state_ == State::kDecoding ||
             state_ == State::kWaitingForProtected ||
             state_ == State::kChangingResolution);
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

void VaapiVideoDecoder::ReturnDecodeSurfaceToPool(
    std::unique_ptr<ScopedVASurface> surface,
    VASurfaceID) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  decode_surface_pool_for_scaling_.push(std::move(surface));
}

}  // namespace media

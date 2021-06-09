// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_video_encode_accelerator.h"

#include <string.h>
#include <va/va.h>
#include <va/va_enc_h264.h>
#include <va/va_enc_vp8.h>

#include <algorithm>
#include <memory>
#include <type_traits>
#include <utility>

#include "base/bind.h"
#include "base/bits.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/cxx17_backports.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/format_utils.h"
#include "media/base/unaligned_shared_memory.h"
#include "media/base/video_bitrate_allocation.h"
#include "media/gpu/chromeos/platform_video_frame_utils.h"
#include "media/gpu/h264_dpb.h"
#include "media/gpu/macros.h"
#include "media/gpu/vaapi/h264_encoder.h"
#include "media/gpu/vaapi/va_surface.h"
#include "media/gpu/vaapi/vaapi_common.h"
#include "media/gpu/vaapi/vaapi_utils.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "media/gpu/vaapi/vp8_vaapi_video_encoder_delegate.h"
#include "media/gpu/vaapi/vp9_temporal_layers.h"
#include "media/gpu/vaapi/vp9_vaapi_video_encoder_delegate.h"
#include "media/gpu/vp8_reference_frame_vector.h"
#include "media/gpu/vp9_reference_frame_vector.h"

#define NOTIFY_ERROR(error, msg)                        \
  do {                                                  \
    SetState(kError);                                   \
    VLOGF(1) << msg;                                    \
    VLOGF(1) << "Calling NotifyError(" << error << ")"; \
    NotifyError(error);                                 \
  } while (0)

namespace media {

namespace {
// Minimum number of frames in flight for pipeline depth, adjust to this number
// if encoder requests less.
constexpr size_t kMinNumFramesInFlight = 4;

void FillVAEncRateControlParams(
    uint32_t bps,
    uint32_t window_size,
    uint32_t initial_qp,
    uint32_t min_qp,
    uint32_t max_qp,
    uint32_t framerate,
    uint32_t buffer_size,
    VAEncMiscParameterRateControl& rate_control_param,
    VAEncMiscParameterFrameRate& framerate_param,
    VAEncMiscParameterHRD& hrd_param) {
  memset(&rate_control_param, 0, sizeof(rate_control_param));
  rate_control_param.bits_per_second = bps;
  rate_control_param.window_size = window_size;
  rate_control_param.initial_qp = initial_qp;
  rate_control_param.min_qp = min_qp;
  rate_control_param.max_qp = max_qp;
  rate_control_param.rc_flags.bits.disable_frame_skip = true;

  memset(&framerate_param, 0, sizeof(framerate_param));
  framerate_param.framerate = framerate;

  memset(&hrd_param, 0, sizeof(hrd_param));
  hrd_param.buffer_size = buffer_size;
  hrd_param.initial_buffer_fullness = buffer_size / 2;
}

// Calculate the size of the allocated buffer aligned to hardware/driver
// requirements.
gfx::Size GetInputFrameSize(VideoPixelFormat format,
                            const gfx::Size& visible_size) {
  // Get a VideoFrameLayout of a graphic buffer with the same gfx::BufferUsage
  // as camera stack.
  absl::optional<VideoFrameLayout> layout = GetPlatformVideoFrameLayout(
      /*gpu_memory_buffer_factory=*/nullptr, format, visible_size,
      gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE);
  if (!layout || layout->planes().empty()) {
    VLOGF(1) << "Failed to allocate VideoFrameLayout";
    return gfx::Size();
  }

  int32_t stride = layout->planes()[0].stride;
  size_t plane_size = layout->planes()[0].size;
  if (stride == 0 || plane_size == 0) {
    VLOGF(1) << "Unexpected stride=" << stride << ", plane_size=" << plane_size;
    return gfx::Size();
  }

  return gfx::Size(stride, plane_size / stride);
}

}  // namespace

struct VaapiVideoEncodeAccelerator::InputFrameRef {
  InputFrameRef(scoped_refptr<VideoFrame> frame, bool force_keyframe)
      : frame(frame), force_keyframe(force_keyframe) {}
  const scoped_refptr<VideoFrame> frame;
  const bool force_keyframe;
};

struct VaapiVideoEncodeAccelerator::BitstreamBufferRef {
  BitstreamBufferRef(int32_t id, BitstreamBuffer buffer)
      : id(id),
        shm(std::make_unique<UnalignedSharedMemory>(buffer.TakeRegion(),
                                                    buffer.size(),
                                                    false)),
        offset(buffer.offset()) {}
  const int32_t id;
  const std::unique_ptr<UnalignedSharedMemory> shm;
  const off_t offset;
};

VideoEncodeAccelerator::SupportedProfiles
VaapiVideoEncodeAccelerator::GetSupportedProfiles() {
  if (IsConfiguredForTesting())
    return supported_profiles_for_testing_;
  return VaapiWrapper::GetSupportedEncodeProfiles();
}

VaapiVideoEncodeAccelerator::VaapiVideoEncodeAccelerator()
    : output_buffer_byte_size_(0),
      state_(kUninitialized),
      child_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      // TODO(akahuang): Change to use SequencedTaskRunner to see if the
      // performance is affected.
      encoder_task_runner_(base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN, base::MayBlock()},
          base::SingleThreadTaskRunnerThreadMode::DEDICATED)) {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(child_sequence_checker_);
  DETACH_FROM_SEQUENCE(encoder_sequence_checker_);

  child_weak_this_ = child_weak_this_factory_.GetWeakPtr();
  encoder_weak_this_ = encoder_weak_this_factory_.GetWeakPtr();

  // The default value of VideoEncoderInfo of VaapiVideoEncodeAccelerator.
  encoder_info_.implementation_name = "VaapiVideoEncodeAccelerator";
  encoder_info_.has_trusted_rate_controller = true;
  DCHECK(encoder_info_.is_hardware_accelerated);
  DCHECK(encoder_info_.supports_native_handle);
  DCHECK(!encoder_info_.supports_simulcast);
}

VaapiVideoEncodeAccelerator::~VaapiVideoEncodeAccelerator() {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
}

bool VaapiVideoEncodeAccelerator::Initialize(const Config& config,
                                             Client* client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(child_sequence_checker_);
  DCHECK_EQ(state_, kUninitialized);
  VLOGF(2) << "Initializing VAVEA, " << config.AsHumanReadableString();

  // VaapiVEA supports temporal layers for VP9 only, but we also allow VP8 to
  // support VP8 simulcast.
  if (config.HasSpatialLayer()) {
    VLOGF(1) << "Spatial layer encoding is not yet supported";
    return false;
  }

  client_ptr_factory_.reset(new base::WeakPtrFactory<Client>(client));
  client_ = client_ptr_factory_->GetWeakPtr();

  VideoCodec codec = VideoCodecProfileToVideoCodec(config.output_profile);
  if (codec != kCodecH264 && codec != kCodecVP8 && codec != kCodecVP9) {
    VLOGF(1) << "Unsupported profile: "
             << GetProfileName(config.output_profile);
    return false;
  }

  switch (config.input_format) {
    case PIXEL_FORMAT_I420:
    case PIXEL_FORMAT_NV12:
      break;
    default:
      VLOGF(1) << "Unsupported input format: " << config.input_format;
      return false;
  }

  if (config.storage_type.value_or(Config::StorageType::kShmem) ==
      Config::StorageType::kGpuMemoryBuffer) {
#if !defined(USE_OZONE)
    VLOGF(1) << "Native mode is only available on OZONE platform.";
    return false;
#else
    if (config.input_format != PIXEL_FORMAT_NV12) {
      // TODO(crbug.com/894381): Support other formats.
      VLOGF(1) << "Unsupported format for native input mode: "
               << VideoPixelFormatToString(config.input_format);
      return false;
    }
    native_input_mode_ = true;
#endif  // USE_OZONE
  }

  const SupportedProfiles& profiles = GetSupportedProfiles();
  auto profile = find_if(profiles.begin(), profiles.end(),
                         [output_profile = config.output_profile](
                             const SupportedProfile& profile) {
                           return profile.profile == output_profile;
                         });
  if (profile == profiles.end()) {
    VLOGF(1) << "Unsupported output profile "
             << GetProfileName(config.output_profile);
    return false;
  }

  if (config.input_visible_size.width() > profile->max_resolution.width() ||
      config.input_visible_size.height() > profile->max_resolution.height()) {
    VLOGF(1) << "Input size too big: " << config.input_visible_size.ToString()
             << ", max supported size: " << profile->max_resolution.ToString();
    return false;
  }

  DCHECK_EQ(IsConfiguredForTesting(), !!vaapi_wrapper_);
  if (!IsConfiguredForTesting()) {
    if (vaapi_wrapper_) {
      VLOGF(1) << "Initialize() is called twice";
      return false;
    }
    VaapiWrapper::CodecMode mode =
        codec == kCodecVP9 ? VaapiWrapper::kEncodeConstantQuantizationParameter
                           : VaapiWrapper::kEncode;
    vaapi_wrapper_ = VaapiWrapper::CreateForVideoCodec(
        mode, config.output_profile, EncryptionScheme::kUnencrypted,
        base::BindRepeating(&ReportVaapiErrorToUMA,
                            "Media.VaapiVideoEncodeAccelerator.VAAPIError"));
    if (!vaapi_wrapper_) {
      VLOGF(1) << "Failed initializing VAAPI for profile "
               << GetProfileName(config.output_profile);
      return false;
    }
  }

  // Finish remaining initialization on the encoder thread.
  encoder_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VaapiVideoEncodeAccelerator::InitializeTask,
                                encoder_weak_this_, config));
  return true;
}

void VaapiVideoEncodeAccelerator::InitializeTask(const Config& config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  DCHECK_EQ(state_, kUninitialized);
  VLOGF(2);

  output_codec_ = VideoCodecProfileToVideoCodec(config.output_profile);
  VaapiVideoEncoderDelegate::Config ave_config{};
  DCHECK_EQ(IsConfiguredForTesting(), !!encoder_);
  // Base::Unretained(this) is safe because |error_cb| is called by
  // |encoder_| and |this| outlives |encoder_|.
  auto error_cb = base::BindRepeating(
      [](VaapiVideoEncodeAccelerator* const vea) {
        vea->SetState(kError);
        vea->NotifyError(kPlatformFailureError);
      },
      base::Unretained(this));
  switch (output_codec_) {
    case kCodecH264:
      if (!IsConfiguredForTesting())
        encoder_ = std::make_unique<H264Encoder>(vaapi_wrapper_, error_cb);

      DCHECK_EQ(ave_config.bitrate_control,
                VaapiVideoEncoderDelegate::BitrateControl::kConstantBitrate);
      break;
    case kCodecVP8:
      if (!IsConfiguredForTesting())
        encoder_ = std::make_unique<VP8VaapiVideoEncoderDelegate>(
            vaapi_wrapper_, error_cb);

      DCHECK_EQ(ave_config.bitrate_control,
                VaapiVideoEncoderDelegate::BitrateControl::kConstantBitrate);
      break;
    case kCodecVP9:
      if (!IsConfiguredForTesting())
        encoder_ = std::make_unique<VP9VaapiVideoEncoderDelegate>(
            vaapi_wrapper_, error_cb);

      ave_config.bitrate_control = VaapiVideoEncoderDelegate::BitrateControl::
          kConstantQuantizationParameter;
      break;
    default:
      NOTREACHED() << "Unsupported codec type " << GetCodecName(output_codec_);
      return;
  }

  if (!vaapi_wrapper_->GetVAEncMaxNumOfRefFrames(
          config.output_profile, &ave_config.max_num_ref_frames)) {
    NOTIFY_ERROR(kPlatformFailureError,
                 "Failed getting max number of reference frames"
                 "supported by the driver");
    return;
  }
  DCHECK_GT(ave_config.max_num_ref_frames, 0u);
  if (!encoder_->Initialize(config, ave_config)) {
    NOTIFY_ERROR(kInvalidArgumentError, "Failed initializing encoder");
    return;
  }

  output_buffer_byte_size_ = encoder_->GetBitstreamBufferSize();

  va_surface_release_cb_ = BindToCurrentLoop(base::BindRepeating(
      &VaapiVideoEncodeAccelerator::RecycleVASurfaceID, encoder_weak_this_));
  vpp_va_surface_release_cb_ = BindToCurrentLoop(base::BindRepeating(
      &VaapiVideoEncodeAccelerator::RecycleVPPVASurfaceID, encoder_weak_this_));

  visible_rect_ = gfx::Rect(config.input_visible_size);
  expected_input_coded_size_ = VideoFrame::DetermineAlignedSize(
      config.input_format, config.input_visible_size);
  DCHECK(
      expected_input_coded_size_.width() <= encoder_->GetCodedSize().width() &&
      expected_input_coded_size_.height() <= encoder_->GetCodedSize().height());

  DCHECK_EQ(IsConfiguredForTesting(), !aligned_va_surface_size_.IsEmpty());
  if (!IsConfiguredForTesting()) {
    // The aligned VA surface size must be the same as a size of a native
    // graphics buffer. Since the VA surface's format is NV12, we specify NV12
    // to query the size of the native graphics buffer.
    aligned_va_surface_size_ =
        GetInputFrameSize(PIXEL_FORMAT_NV12, config.input_visible_size);
    if (aligned_va_surface_size_.IsEmpty()) {
      NOTIFY_ERROR(kPlatformFailureError, "Failed to get frame size");
      return;
    }
  }

  va_surfaces_per_video_frame_ =
      native_input_mode_
          ?
          // In native input mode, we do not need surfaces for input frames.
          kNumSurfacesForOutputPicture
          :
          // In non-native mode, we need to create additional surfaces for input
          // frames.
          kNumSurfacesForOutputPicture + kNumSurfacesPerInputVideoFrame;

  // The number of required buffers is the number of required reference frames
  // + 1 for the current frame to be encoded.
  const size_t max_ref_frames = encoder_->GetMaxNumOfRefFrames();
  num_frames_in_flight_ = std::max(kMinNumFramesInFlight, max_ref_frames);
  DVLOGF(1) << "Frames in flight: " << num_frames_in_flight_;

  // The surface size for the reconstructed surface (and input surface in non
  // native input mode) is the coded size.
  if (!vaapi_wrapper_->CreateContextAndSurfaces(
          kVaSurfaceFormat, encoder_->GetCodedSize(),
          VaapiWrapper::SurfaceUsageHint::kVideoEncoder,
          (num_frames_in_flight_ + 1) * va_surfaces_per_video_frame_,
          &available_va_surface_ids_)) {
    NOTIFY_ERROR(kPlatformFailureError, "Failed creating VASurfaces");
    return;
  }

  child_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Client::RequireBitstreamBuffers, client_,
                     num_frames_in_flight_, expected_input_coded_size_,
                     output_buffer_byte_size_));

  if (config.HasTemporalLayer()) {
    DCHECK(!config.spatial_layers.empty());
    encoder_info_.fps_allocation[0] = VP9TemporalLayers::GetFpsAllocation(
        config.spatial_layers[0].num_of_temporal_layers);
  } else {
    constexpr uint8_t kFullFramerate = 255;
    encoder_info_.fps_allocation[0] = {kFullFramerate};
  }

  // Notify VideoEncoderInfo after initialization.
  child_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Client::NotifyEncoderInfoChange, client_, encoder_info_));
  SetState(kEncoding);
}

void VaapiVideoEncodeAccelerator::RecycleVASurfaceID(
    VASurfaceID va_surface_id) {
  DVLOGF(4) << "va_surface_id: " << va_surface_id;
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);

  available_va_surface_ids_.push_back(va_surface_id);
  EncodePendingInputs();
}

void VaapiVideoEncodeAccelerator::RecycleVPPVASurfaceID(
    VASurfaceID va_surface_id) {
  DVLOGF(4) << "va_surface_id: " << va_surface_id;
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);

  available_vpp_va_surface_ids_.push_back(va_surface_id);
  EncodePendingInputs();
}

void VaapiVideoEncodeAccelerator::ExecuteEncode(VASurfaceID va_surface_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);

  if (!vaapi_wrapper_->ExecuteAndDestroyPendingBuffers(va_surface_id))
    NOTIFY_ERROR(kPlatformFailureError, "Failed to execute encode");
}

void VaapiVideoEncodeAccelerator::UploadFrame(
    scoped_refptr<VideoFrame> frame,
    VASurfaceID va_surface_id,
    const gfx::Size& va_surface_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);

  DVLOGF(4) << "frame is uploading: " << va_surface_id;
  if (!vaapi_wrapper_->UploadVideoFrameToSurface(*frame, va_surface_id,
                                                 va_surface_size))
    NOTIFY_ERROR(kPlatformFailureError, "Failed to upload frame");
}

void VaapiVideoEncodeAccelerator::TryToReturnBitstreamBuffer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);

  if (state_ != kEncoding)
    return;

  while (!submitted_encode_jobs_.empty() &&
         submitted_encode_jobs_.front() == nullptr) {
    // A null job indicates a flush command.
    submitted_encode_jobs_.pop();
    DVLOGF(2) << "FlushDone";
    DCHECK(flush_callback_);
    child_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(flush_callback_), true));
  }

  if (submitted_encode_jobs_.empty() || available_bitstream_buffers_.empty())
    return;

  auto buffer = std::move(available_bitstream_buffers_.front());
  available_bitstream_buffers_.pop();
  auto encode_job = std::move(submitted_encode_jobs_.front());
  submitted_encode_jobs_.pop();

  ReturnBitstreamBuffer(std::move(encode_job), std::move(buffer));
}

void VaapiVideoEncodeAccelerator::ReturnBitstreamBuffer(
    std::unique_ptr<EncodeJob> encode_job,
    std::unique_ptr<BitstreamBufferRef> buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  const VABufferID coded_buffer_id = encode_job->coded_buffer_id();
  uint8_t* target_data = static_cast<uint8_t*>(buffer->shm->memory());
  size_t data_size = 0;
  if (!vaapi_wrapper_->DownloadFromVABuffer(
          coded_buffer_id, encode_job->input_surface()->id(), target_data,
          buffer->shm->size(), &data_size)) {
    NOTIFY_ERROR(kPlatformFailureError, "Failed downloading coded buffer");
    return;
  }
  DVLOGF(4) << "Returning bitstream buffer "
            << (encode_job->IsKeyframeRequested() ? "(keyframe)" : "")
            << " id: " << buffer->id << " size: " << data_size;

  auto metadata = encoder_->GetMetadata(encode_job.get(), data_size);
  encode_job.reset();

  child_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Client::BitstreamBufferReady, client_,
                                buffer->id, std::move(metadata)));
}

void VaapiVideoEncodeAccelerator::Encode(scoped_refptr<VideoFrame> frame,
                                         bool force_keyframe) {
  DVLOGF(4) << "Frame timestamp: " << frame->timestamp().InMilliseconds()
            << " force_keyframe: " << force_keyframe;
  DCHECK_CALLED_ON_VALID_SEQUENCE(child_sequence_checker_);

  encoder_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VaapiVideoEncodeAccelerator::EncodeTask,
                     encoder_weak_this_, std::move(frame), force_keyframe));
}

void VaapiVideoEncodeAccelerator::EncodeTask(scoped_refptr<VideoFrame> frame,
                                             bool force_keyframe) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  DCHECK_NE(state_, kUninitialized);

  input_queue_.push(
      std::make_unique<InputFrameRef>(std::move(frame), force_keyframe));
  EncodePendingInputs();
}

std::unique_ptr<VaapiVideoEncoderDelegate::EncodeJob>
VaapiVideoEncodeAccelerator::CreateEncodeJob(scoped_refptr<VideoFrame> frame,
                                             bool force_keyframe) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);

  if (native_input_mode_ &&
      frame->storage_type() != VideoFrame::STORAGE_DMABUFS &&
      frame->storage_type() != VideoFrame::STORAGE_GPU_MEMORY_BUFFER) {
    NOTIFY_ERROR(kPlatformFailureError,
                 "Unexpected storage: " << frame->storage_type());
    return nullptr;
  }

  if (available_va_surface_ids_.size() < va_surfaces_per_video_frame_ ||
      (vpp_vaapi_wrapper_ && available_vpp_va_surface_ids_.empty())) {
    DVLOGF(4) << "Not enough surfaces available";
    return nullptr;
  }

  auto coded_buffer = vaapi_wrapper_->CreateVABuffer(VAEncCodedBufferType,
                                                     output_buffer_byte_size_);
  if (!coded_buffer) {
    NOTIFY_ERROR(kPlatformFailureError, "Failed creating coded buffer");
    return nullptr;
  }

  scoped_refptr<VASurface> input_surface;
  if (native_input_mode_) {
    if (frame->format() != PIXEL_FORMAT_NV12) {
      NOTIFY_ERROR(kPlatformFailureError,
                   "Expected NV12, got: " << frame->format());
      return nullptr;
    }
    DCHECK(frame);

    scoped_refptr<gfx::NativePixmap> pixmap =
        CreateNativePixmapDmaBuf(frame.get());
    if (!pixmap) {
      NOTIFY_ERROR(kPlatformFailureError,
                   "Failed to create NativePixmap from VideoFrame");
      return nullptr;
    }
    input_surface = vaapi_wrapper_->CreateVASurfaceForPixmap(std::move(pixmap));

    if (!input_surface) {
      NOTIFY_ERROR(kPlatformFailureError, "Failed to create VASurface");
      return nullptr;
    }
  } else {
    if (expected_input_coded_size_ != frame->coded_size()) {
      // In non-zero copy mode, the coded size of the incoming frame should be
      // the same as the one we requested through
      // Client::RequireBitstreamBuffers().
      NOTIFY_ERROR(kPlatformFailureError,
                   "Expected frame coded size: "
                       << expected_input_coded_size_.ToString()
                       << ", but got: " << frame->coded_size().ToString());
      return nullptr;
    }

    DCHECK_EQ(visible_rect_.origin(), gfx::Point(0, 0));
    if (visible_rect_ != frame->visible_rect()) {
      // In non-zero copy mode, the client is responsible for scaling and
      // cropping.
      NOTIFY_ERROR(kPlatformFailureError,
                   "Expected frame visible rectangle: "
                       << visible_rect_.ToString()
                       << ", but got: " << frame->visible_rect().ToString());
      return nullptr;
    }
    input_surface = new VASurface(available_va_surface_ids_.back(),
                                  encoder_->GetCodedSize(), kVaSurfaceFormat,
                                  base::BindOnce(va_surface_release_cb_));
    available_va_surface_ids_.pop_back();
  }

  if (visible_rect_ != frame->visible_rect()) {
    DCHECK(native_input_mode_);
    // Do cropping/scaling.  Here the buffer size contained in |input_surface|
    // is |frame->coded_size()|.
    if (!vpp_vaapi_wrapper_) {
      vpp_vaapi_wrapper_ = VaapiWrapper::Create(
          VaapiWrapper::kVideoProcess, VAProfileNone,
          EncryptionScheme::kUnencrypted,
          base::BindRepeating(
              &ReportVaapiErrorToUMA,
              "Media.VaapiVideoEncodeAccelerator.Vpp.VAAPIError"));
      if (!vpp_vaapi_wrapper_) {
        NOTIFY_ERROR(kPlatformFailureError,
                     "Failed to initialize VppVaapiWrapper");
        return nullptr;
      }

      // Allocate the same number of surfaces as reconstructed surfaces.
      if (!vpp_vaapi_wrapper_->CreateContextAndSurfaces(
              kVaSurfaceFormat, aligned_va_surface_size_,
              VaapiWrapper::SurfaceUsageHint::kVideoProcessWrite,
              num_frames_in_flight_ + 1, &available_vpp_va_surface_ids_)) {
        NOTIFY_ERROR(kPlatformFailureError,
                     "Failed creating VASurfaces for scaling");
        vpp_vaapi_wrapper_ = nullptr;
        return nullptr;
      };
    }
    scoped_refptr<VASurface> blit_surface = new VASurface(
        available_vpp_va_surface_ids_.back(), aligned_va_surface_size_,
        kVaSurfaceFormat, base::BindOnce(vpp_va_surface_release_cb_));
    available_vpp_va_surface_ids_.pop_back();
    // Crop/Scale the visible area of |frame->visible_rect()| ->
    // |visible_rect_|.
    if (!vpp_vaapi_wrapper_->BlitSurface(*input_surface, *blit_surface,
                                         frame->visible_rect(),
                                         visible_rect_)) {
      NOTIFY_ERROR(
          kPlatformFailureError,
          "Failed BlitSurface on frame size: "
              << frame->coded_size().ToString()
              << " (visible rect: " << frame->visible_rect().ToString()
              << ") -> frame size: " << aligned_va_surface_size_.ToString()
              << " (visible rect: " << visible_rect_.ToString() << ")");
      return nullptr;
    }
    // We can destroy the original |input_surface| because the buffer is already
    // copied to blit_surface.
    input_surface = std::move(blit_surface);
  }

  // Here, the surface size contained in |input_surface| is
  // |aligned_va_surface_size_| regardless of scaling in zero-copy mode, and
  // encoder_->GetCodedSize().
  scoped_refptr<VASurface> reconstructed_surface =
      new VASurface(available_va_surface_ids_.back(), encoder_->GetCodedSize(),
                    kVaSurfaceFormat, base::BindOnce(va_surface_release_cb_));
  available_va_surface_ids_.pop_back();

  scoped_refptr<CodecPicture> picture;
  switch (output_codec_) {
    case kCodecH264:
      picture = new VaapiH264Picture(std::move(reconstructed_surface));
      break;
    case kCodecVP8:
      picture = new VaapiVP8Picture(std::move(reconstructed_surface));
      break;
    case kCodecVP9:
      picture = new VaapiVP9Picture(std::move(reconstructed_surface));
      break;
    default:
      return nullptr;
  }

  auto job = std::make_unique<EncodeJob>(
      frame, force_keyframe,
      base::BindOnce(&VaapiVideoEncodeAccelerator::ExecuteEncode,
                     encoder_weak_this_, input_surface->id()),
      input_surface, std::move(picture), std::move(coded_buffer));

  if (!native_input_mode_) {
    job->AddSetupCallback(base::BindOnce(
        &VaapiVideoEncodeAccelerator::UploadFrame, encoder_weak_this_, frame,
        input_surface->id(), input_surface->size()));
  }

  return job;
}

void VaapiVideoEncodeAccelerator::EncodePendingInputs() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  DVLOGF(4);

  while (state_ == kEncoding && !input_queue_.empty()) {
    const std::unique_ptr<InputFrameRef>& input_frame = input_queue_.front();

    // If this is a flush (null) frame, don't create/submit a new encode job for
    // it, but forward a null job to the submitted_encode_jobs_ queue.
    std::unique_ptr<EncodeJob> job;
    TRACE_EVENT0("media,gpu", "VAVEA::FromCreateEncodeJobToReturn");
    if (input_frame) {
      job = CreateEncodeJob(input_frame->frame, input_frame->force_keyframe);
      if (!job)
        return;
    }

    input_queue_.pop();

    if (job && !encoder_->PrepareEncodeJob(job.get())) {
      NOTIFY_ERROR(kPlatformFailureError, "Failed preparing an encode job.");
      return;
    }

    TRACE_EVENT0("media,gpu", "VAVEA::FromExecuteToReturn");
    if (job) {
      TRACE_EVENT0("media,gpu", "VAVEA::Execute");
      job->Execute();
    }

    submitted_encode_jobs_.push(std::move(job));
    TryToReturnBitstreamBuffer();
  }
}

void VaapiVideoEncodeAccelerator::UseOutputBitstreamBuffer(
    BitstreamBuffer buffer) {
  DVLOGF(4) << "id: " << buffer.id();
  DCHECK_CALLED_ON_VALID_SEQUENCE(child_sequence_checker_);

  if (buffer.size() < output_buffer_byte_size_) {
    NOTIFY_ERROR(kInvalidArgumentError, "Provided bitstream buffer too small");
    return;
  }

  auto buffer_ref =
      std::make_unique<BitstreamBufferRef>(buffer.id(), std::move(buffer));

  encoder_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VaapiVideoEncodeAccelerator::UseOutputBitstreamBufferTask,
                     encoder_weak_this_, std::move(buffer_ref)));
}

void VaapiVideoEncodeAccelerator::UseOutputBitstreamBufferTask(
    std::unique_ptr<BitstreamBufferRef> buffer_ref) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  DCHECK_NE(state_, kUninitialized);

  if (!buffer_ref->shm->MapAt(buffer_ref->offset, buffer_ref->shm->size())) {
    NOTIFY_ERROR(kPlatformFailureError, "Failed mapping shared memory.");
    return;
  }

  available_bitstream_buffers_.push(std::move(buffer_ref));
  TryToReturnBitstreamBuffer();
}

void VaapiVideoEncodeAccelerator::RequestEncodingParametersChange(
    uint32_t bitrate,
    uint32_t framerate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(child_sequence_checker_);

  VideoBitrateAllocation allocation;
  allocation.SetBitrate(0, 0, bitrate);
  encoder_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &VaapiVideoEncodeAccelerator::RequestEncodingParametersChangeTask,
          encoder_weak_this_, allocation, framerate));
}

void VaapiVideoEncodeAccelerator::RequestEncodingParametersChange(
    const VideoBitrateAllocation& bitrate_allocation,
    uint32_t framerate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(child_sequence_checker_);

  encoder_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &VaapiVideoEncodeAccelerator::RequestEncodingParametersChangeTask,
          encoder_weak_this_, bitrate_allocation, framerate));
}

void VaapiVideoEncodeAccelerator::RequestEncodingParametersChangeTask(
    VideoBitrateAllocation bitrate_allocation,
    uint32_t framerate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  DCHECK_NE(state_, kUninitialized);

  if (!encoder_->UpdateRates(bitrate_allocation, framerate)) {
    VLOGF(1) << "Failed to update rates to " << bitrate_allocation.GetSumBps()
             << " " << framerate;
  }
}

void VaapiVideoEncodeAccelerator::Flush(FlushCallback flush_callback) {
  DVLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(child_sequence_checker_);

  encoder_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VaapiVideoEncodeAccelerator::FlushTask,
                                encoder_weak_this_, std::move(flush_callback)));
}

void VaapiVideoEncodeAccelerator::FlushTask(FlushCallback flush_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);

  if (flush_callback_) {
    NOTIFY_ERROR(kIllegalStateError, "There is a pending flush");
    child_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(flush_callback), false));
    return;
  }
  flush_callback_ = std::move(flush_callback);

  // Insert an null job to indicate a flush command.
  input_queue_.push(std::unique_ptr<InputFrameRef>(nullptr));
  EncodePendingInputs();
}

bool VaapiVideoEncodeAccelerator::IsFlushSupported() {
  return true;
}

void VaapiVideoEncodeAccelerator::Destroy() {
  DVLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(child_sequence_checker_);

  child_weak_this_factory_.InvalidateWeakPtrs();

  // We're destroying; cancel all callbacks.
  client_ptr_factory_.reset();

  encoder_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VaapiVideoEncodeAccelerator::DestroyTask,
                                encoder_weak_this_));
}

void VaapiVideoEncodeAccelerator::DestroyTask() {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);

  encoder_weak_this_factory_.InvalidateWeakPtrs();

  if (flush_callback_) {
    child_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(flush_callback_), false));
  }

  // Clean up members that are to be accessed on the encoder thread only.
  if (vaapi_wrapper_)
    vaapi_wrapper_->DestroyContextAndSurfaces(available_va_surface_ids_);
  if (vpp_vaapi_wrapper_) {
    vpp_vaapi_wrapper_->DestroyContextAndSurfaces(
        available_vpp_va_surface_ids_);
  }

  available_va_buffer_ids_.clear();

  while (!available_bitstream_buffers_.empty())
    available_bitstream_buffers_.pop();

  while (!input_queue_.empty())
    input_queue_.pop();

  // Note ScopedVABuffer in EncodeJob must be destroyed before
  // |vaapi_wrapper_| is destroyed to ensure VADisplay is valid on the
  // ScopedVABuffer's destruction.
  DCHECK(vaapi_wrapper_ || submitted_encode_jobs_.empty());
  while (!submitted_encode_jobs_.empty())
    submitted_encode_jobs_.pop();

  encoder_ = nullptr;

  delete this;
}

void VaapiVideoEncodeAccelerator::SetState(State state) {
  // Only touch state on encoder thread, unless it's not running.
  if (!encoder_task_runner_->BelongsToCurrentThread()) {
    encoder_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&VaapiVideoEncodeAccelerator::SetState,
                                  encoder_weak_this_, state));
    return;
  }

  VLOGF(2) << "setting state to: " << state;
  state_ = state;
}

void VaapiVideoEncodeAccelerator::NotifyError(Error error) {
  if (!child_task_runner_->BelongsToCurrentThread()) {
    child_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&VaapiVideoEncodeAccelerator::NotifyError,
                                  child_weak_this_, error));
    return;
  }

  if (client_) {
    client_->NotifyError(error);
    client_ptr_factory_.reset();
  }
}

}  // namespace media

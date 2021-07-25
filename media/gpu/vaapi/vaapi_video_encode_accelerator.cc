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
#include "base/containers/contains.h"
#include "base/cxx17_backports.h"
#include "base/feature_list.h"
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
#include "media/base/media_switches.h"
#include "media/base/unaligned_shared_memory.h"
#include "media/base/video_bitrate_allocation.h"
#include "media/gpu/chromeos/platform_video_frame_utils.h"
#include "media/gpu/h264_dpb.h"
#include "media/gpu/macros.h"
#include "media/gpu/vaapi/h264_vaapi_video_encoder_delegate.h"
#include "media/gpu/vaapi/va_surface.h"
#include "media/gpu/vaapi/vaapi_common.h"
#include "media/gpu/vaapi/vaapi_utils.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "media/gpu/vaapi/vp8_vaapi_video_encoder_delegate.h"
#include "media/gpu/vaapi/vp9_svc_layers.h"
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

// VASurfaceIDs internal format.
constexpr unsigned int kVaSurfaceFormat = VA_RT_FORMAT_YUV420;

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

  if (config.HasSpatialLayer()) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    if (!base::FeatureList::IsEnabled(kVaapiVp9kSVCHWEncoding) &&
        !IsConfiguredForTesting()) {
      VLOGF(1) << "Spatial layer encoding is not yet enabled by default";
      return false;
    }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    if (config.inter_layer_pred != Config::InterLayerPredMode::kOnKeyPic) {
      VLOGF(1) << "Only K-SVC encoding is supported.";
      return false;
    }

    if (config.output_profile != VideoCodecProfile::VP9PROFILE_PROFILE0) {
      VLOGF(1) << "Spatial layers are only supported for VP9 encoding";
      return false;
    }

    // TODO(crbug.com/1186051): Remove this restriction.
    for (size_t i = 0; i < config.spatial_layers.size(); ++i) {
      for (size_t j = i + 1; j < config.spatial_layers.size(); ++j) {
        if (config.spatial_layers[i].width == config.spatial_layers[j].width &&
            config.spatial_layers[i].height ==
                config.spatial_layers[j].height) {
          VLOGF(1) << "Doesn't support k-SVC encoding where spatial layers "
                      "have the same resolution";
          return false;
        }
      }
    }

    if (!IsConfiguredForTesting()) {
      VAProfile va_profile = VAProfileVP9Profile0;
      if (VaapiWrapper::GetDefaultVaEntryPoint(
              VaapiWrapper::kEncodeConstantQuantizationParameter, va_profile) !=
          VAEntrypointEncSliceLP) {
        VLOGF(1) << "Currently spatial layer encoding is only supported by "
                    "VAEntrypointEncSliceLP";
        return false;
      }
    }
  }

  client_ptr_factory_.reset(new base::WeakPtrFactory<Client>(client));
  client_ = client_ptr_factory_->GetWeakPtr();

  const VideoCodec codec = VideoCodecProfileToVideoCodec(config.output_profile);
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

  if (config.HasSpatialLayer() && !native_input_mode_) {
    VLOGF(1) << "Spatial scalability is only supported for native input now";
    return false;
  }

  const SupportedProfiles& profiles = GetSupportedProfiles();
  const auto profile = find_if(profiles.begin(), profiles.end(),
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
                           : VaapiWrapper::kEncodeConstantBitrate;
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
      if (!IsConfiguredForTesting()) {
        encoder_ = std::make_unique<H264VaapiVideoEncoderDelegate>(
            vaapi_wrapper_, error_cb);
      }

      DCHECK_EQ(ave_config.bitrate_control,
                VaapiVideoEncoderDelegate::BitrateControl::kConstantBitrate);
      break;
    case kCodecVP8:
      if (!IsConfiguredForTesting()) {
        encoder_ = std::make_unique<VP8VaapiVideoEncoderDelegate>(
            vaapi_wrapper_, error_cb);
      }

      DCHECK_EQ(ave_config.bitrate_control,
                VaapiVideoEncoderDelegate::BitrateControl::kConstantBitrate);
      break;
    case kCodecVP9:
      if (!IsConfiguredForTesting()) {
        encoder_ = std::make_unique<VP9VaapiVideoEncoderDelegate>(
            vaapi_wrapper_, error_cb);
      }

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

  visible_rect_ = gfx::Rect(config.input_visible_size);
  expected_input_coded_size_ = VideoFrame::DetermineAlignedSize(
      config.input_format, config.input_visible_size);
  DCHECK(
      expected_input_coded_size_.width() <= encoder_->GetCodedSize().width() &&
      expected_input_coded_size_.height() <= encoder_->GetCodedSize().height());

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
  available_va_surfaces_ = vaapi_wrapper_->CreateContextAndScopedVASurfaces(
      kVaSurfaceFormat, encoder_->GetCodedSize(),
      {VaapiWrapper::SurfaceUsageHint::kVideoEncoder},
      (num_frames_in_flight_ + 1) * va_surfaces_per_video_frame_,
      /*visible_size=*/absl::nullopt);
  if (available_va_surfaces_.empty()) {
    NOTIFY_ERROR(kPlatformFailureError, "Failed creating VASurfaces");
    return;
  }

  child_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Client::RequireBitstreamBuffers, client_,
                     num_frames_in_flight_, expected_input_coded_size_,
                     output_buffer_byte_size_));

  if (config.HasSpatialLayer() || config.HasTemporalLayer()) {
    DCHECK(!config.spatial_layers.empty());
    for (size_t i = 0; i < config.spatial_layers.size(); ++i) {
      encoder_info_.fps_allocation[i] = VP9SVCLayers::GetFpsAllocation(
          config.spatial_layers[i].num_of_temporal_layers);
    }
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

void VaapiVideoEncodeAccelerator::RecycleVASurface(
    std::vector<std::unique_ptr<ScopedVASurface>>* va_surfaces,
    std::unique_ptr<ScopedVASurface> va_surface,
    VASurfaceID va_surface_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  DCHECK(va_surface);
  DCHECK_EQ(va_surface_id, va_surface->id());
  DVLOGF(4) << "va_surface_id: " << va_surface_id;

  va_surfaces->push_back(std::move(va_surface));
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
                                                 va_surface_size)) {
    NOTIFY_ERROR(kPlatformFailureError, "Failed to upload frame");
  }
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

scoped_refptr<VASurface>
VaapiVideoEncodeAccelerator::GetAvailableVASurfaceAsRefCounted(
    std::vector<std::unique_ptr<ScopedVASurface>>* va_surfaces) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  DCHECK(va_surfaces && !va_surfaces->empty());
  auto scoped_va_surface = std::move(va_surfaces->back());
  const VASurfaceID id = scoped_va_surface->id();
  const gfx::Size& size = scoped_va_surface->size();
  const unsigned int format = scoped_va_surface->format();

  VASurface::ReleaseCB release_cb = BindToCurrentLoop(base::BindOnce(
      &VaapiVideoEncodeAccelerator::RecycleVASurface, encoder_weak_this_,
      va_surfaces, std::move(scoped_va_surface)));

  va_surfaces->pop_back();
  return base::MakeRefCounted<VASurface>(id, size, format,
                                         std::move(release_cb));
}

scoped_refptr<VASurface>
VaapiVideoEncodeAccelerator::BlitSurfaceWithCreateVppIfNeeded(
    const VASurface& input_surface,
    const gfx::Rect& input_visible_rect,
    const gfx::Size& encode_size,
    size_t num_va_surfaces) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);

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

    // VA context for VPP is not associated with a specific resolution.
    if (!vpp_vaapi_wrapper_->CreateContext(gfx::Size())) {
      NOTIFY_ERROR(kPlatformFailureError, "Failed creating Context for VPP");
      return nullptr;
    }
  }

  if (!base::Contains(available_vpp_va_surfaces_, encode_size)) {
    auto scoped_va_surfaces = vpp_vaapi_wrapper_->CreateScopedVASurfaces(
        kVaSurfaceFormat, encode_size,
        {VaapiWrapper::SurfaceUsageHint::kVideoProcessWrite,
         VaapiWrapper::SurfaceUsageHint::kVideoEncoder},
        num_va_surfaces, /*visible_size=*/absl::nullopt,
        /*va_fourcc=*/absl::nullopt);
    if (scoped_va_surfaces.empty()) {
      NOTIFY_ERROR(kPlatformFailureError, "Failed creating VASurfaces");
      return nullptr;
    }

    available_vpp_va_surfaces_[encode_size] = std::move(scoped_va_surfaces);
  }

  auto blit_surface = GetAvailableVASurfaceAsRefCounted(
      &available_vpp_va_surfaces_[encode_size]);
  if (!vpp_vaapi_wrapper_->BlitSurface(input_surface, *blit_surface,
                                       input_visible_rect,
                                       gfx::Rect(encode_size))) {
    NOTIFY_ERROR(kPlatformFailureError,
                 "Failed BlitSurface on frame size: "
                     << input_surface.size().ToString()
                     << " (visible rect: " << input_visible_rect.ToString()
                     << ") -> encode size: " << encode_size.ToString());
    return nullptr;
  }

  return blit_surface;
}

bool VaapiVideoEncodeAccelerator::CreateSurfacesForGpuMemoryBufferEncoding(
    const VideoFrame& frame,
    const gfx::Size& encode_size,
    scoped_refptr<VASurface>* input_surface,
    scoped_refptr<VASurface>* reconstructed_surface) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  DCHECK(native_input_mode_);

  if (frame.storage_type() != VideoFrame::STORAGE_GPU_MEMORY_BUFFER) {
    NOTIFY_ERROR(kPlatformFailureError,
                 "Unexpected storage: "
                     << VideoFrame::StorageTypeToString(frame.storage_type()));
    return false;
  }

  if (frame.format() != PIXEL_FORMAT_NV12) {
    NOTIFY_ERROR(
        kPlatformFailureError,
        "Expected NV12, got: " << VideoPixelFormatToString(frame.format()));
    return false;
  }

  const bool do_vpp = frame.visible_rect() != gfx::Rect(encode_size);
  if (do_vpp) {
    constexpr size_t kNumSurfaces = 2;  // For input and reconstructed surface.
    if (base::Contains(available_vpp_va_surfaces_, encode_size) &&
        available_vpp_va_surfaces_[encode_size].size() < kNumSurfaces) {
      DVLOGF(4) << "Not enough surfaces available";
      return false;
    }
  } else {
    if (available_va_surfaces_.empty()) {  // For the reconstructed surface.
      DVLOGF(4) << "Not surface available";
      return false;
    }
  }

  // Create VASurface from GpuMemory-based VideoFrame.
  scoped_refptr<gfx::NativePixmap> pixmap = CreateNativePixmapDmaBuf(&frame);
  if (!pixmap) {
    NOTIFY_ERROR(kPlatformFailureError,
                 "Failed to create NativePixmap from VideoFrame");
    return false;
  }

  *input_surface = vaapi_wrapper_->CreateVASurfaceForPixmap(std::move(pixmap));
  if (!(*input_surface)) {
    NOTIFY_ERROR(kPlatformFailureError, "Failed to create VASurface");
    return false;
  }

  // Crop and Scale input surface to a surface whose size is |encode_size|.
  // The size of a reconstructed surface is also |encode_size|.
  if (do_vpp) {
    // Create blit destination and reconstructed surfaces.
    *input_surface = BlitSurfaceWithCreateVppIfNeeded(
        *input_surface->get(), frame.visible_rect(), encode_size,
        (num_frames_in_flight_ + 1) * 2);
    if (!(*input_surface))
      return false;
    // A reconstructed surface is fine to be created by VPP VaapiWrapper and
    // encoder VaapiWrapper. Use one created by VPP VaapiWrapper when VPP is
    // executed.
    *reconstructed_surface = GetAvailableVASurfaceAsRefCounted(
        &available_vpp_va_surfaces_[encode_size]);
  } else {
    // TODO(crbug.com/1186051): Create VA Surface here for the first time, not
    // in Initialize()
    *reconstructed_surface =
        GetAvailableVASurfaceAsRefCounted(&available_va_surfaces_);
  }

  DCHECK(*input_surface);
  return !!(*reconstructed_surface);
}

bool VaapiVideoEncodeAccelerator::CreateSurfacesForShmemEncoding(
    const VideoFrame& frame,
    scoped_refptr<VASurface>* input_surface,
    scoped_refptr<VASurface>* reconstructed_surface) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  DCHECK(!native_input_mode_);
  DCHECK(frame.IsMappable());

  if (expected_input_coded_size_ != frame.coded_size()) {
    // In non-zero copy mode, the coded size of the incoming frame should be
    // the same as the one we requested through
    // Client::RequireBitstreamBuffers().
    NOTIFY_ERROR(kPlatformFailureError,
                 "Expected frame coded size: "
                     << expected_input_coded_size_.ToString()
                     << ", but got: " << frame.coded_size().ToString());
    return false;
  }

  DCHECK_EQ(visible_rect_.origin(), gfx::Point(0, 0));
  if (visible_rect_ != frame.visible_rect()) {
    // In non-zero copy mode, the client is responsible for scaling and
    // cropping.
    NOTIFY_ERROR(kPlatformFailureError,
                 "Expected frame visible rectangle: "
                     << visible_rect_.ToString()
                     << ", but got: " << frame.visible_rect().ToString());
    return false;
  }

  constexpr size_t kNumSurfaces = 2;  // input and reconstructed surface.
  if (available_va_surfaces_.size() < kNumSurfaces) {
    DVLOGF(4) << "Not enough surfaces available";
    return false;
  }

  *input_surface = GetAvailableVASurfaceAsRefCounted(&available_va_surfaces_);
  *reconstructed_surface =
      GetAvailableVASurfaceAsRefCounted(&available_va_surfaces_);
  return true;
}

std::unique_ptr<VaapiVideoEncoderDelegate::EncodeJob>
VaapiVideoEncodeAccelerator::CreateEncodeJob(scoped_refptr<VideoFrame> frame,
                                             bool force_keyframe,
                                             const gfx::Size& encode_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  DCHECK(frame);

  scoped_refptr<VASurface> input_surface;
  scoped_refptr<VASurface> reconstructed_surface;
  if (native_input_mode_) {
    if (!CreateSurfacesForGpuMemoryBufferEncoding(
            *frame, encode_size, &input_surface, &reconstructed_surface)) {
      return nullptr;
    }
  } else {
    if (!CreateSurfacesForShmemEncoding(*frame, &input_surface,
                                        &reconstructed_surface)) {
      return nullptr;
    }
  }
  DCHECK(input_surface && reconstructed_surface);

  auto coded_buffer = vaapi_wrapper_->CreateVABuffer(VAEncCodedBufferType,
                                                     output_buffer_byte_size_);
  if (!coded_buffer) {
    NOTIFY_ERROR(kPlatformFailureError, "Failed creating coded buffer");
    return nullptr;
  }
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

  std::vector<gfx::Size> spatial_layer_resolutions =
      encoder_->GetSVCLayerResolutions();
  if (spatial_layer_resolutions.empty()) {
    VLOGF(1) << " Failed to get SVC layer resolutions";
    return;
  }

  while (state_ == kEncoding && !input_queue_.empty()) {
    const std::unique_ptr<InputFrameRef>& input_frame = input_queue_.front();
    if (!input_frame) {
      // If this is a flush (null) frame, don't create/submit a new encode job
      // for it, but forward a null job to the |submitted_encode_jobs_| queue.
      submitted_encode_jobs_.push(nullptr);
      input_queue_.pop();
      TryToReturnBitstreamBuffer();
      continue;
    }

    // Encoding different spatial layers for |input_frame|.
    std::vector<std::unique_ptr<EncodeJob>> jobs;
    for (size_t spatial_idx = 0; spatial_idx < spatial_layer_resolutions.size();
         ++spatial_idx) {
      std::unique_ptr<EncodeJob> job;
      TRACE_EVENT0("media,gpu", "VAVEA::FromCreateEncodeJobToReturn");
      const bool force_key =
          (spatial_idx == 0 ? input_frame->force_keyframe : false);
      job = CreateEncodeJob(input_frame->frame, force_key,
                            spatial_layer_resolutions[spatial_idx]);
      if (!job)
        return;

      jobs.emplace_back(std::move(job));
    }

    for (auto&& job : jobs) {
      if (!encoder_->PrepareEncodeJob(job.get())) {
        NOTIFY_ERROR(kPlatformFailureError, "Failed preparing an encode job.");
        return;
      }

      TRACE_EVENT0("media,gpu", "VAVEA::FromExecuteToReturn");
      TRACE_EVENT0("media,gpu", "VAVEA::Execute");
      job->Execute();

      submitted_encode_jobs_.push(std::move(job));
      TryToReturnBitstreamBuffer();
    }

    input_queue_.pop();
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
    const Bitrate& bitrate,
    uint32_t framerate) {
  // If this is changed to use variable bitrate encoding, change the mode check
  // to check that the mode matches the current mode.
  if (bitrate.mode() != Bitrate::Mode::kConstant) {
    VLOGF(1) << "Failed to update rates due to invalid bitrate mode.";
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(child_sequence_checker_);

  VideoBitrateAllocation allocation;
  allocation.SetBitrate(0, 0, bitrate.target());
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
  // Call DestroyContext() explicitly to make sure it's destroyed before
  // VA surfaces.
  if (vaapi_wrapper_)
    vaapi_wrapper_->DestroyContext();

  available_va_surfaces_.clear();
  available_va_buffer_ids_.clear();

  if (vpp_vaapi_wrapper_)
    vpp_vaapi_wrapper_->DestroyContext();

  available_vpp_va_surfaces_.clear();

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

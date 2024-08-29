// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_video_encode_accelerator.h"

#include <string.h>
#include <va/va.h>

#include <algorithm>
#include <memory>
#include <type_traits>
#include <utility>
#include <variant>

#include "base/bits.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "media/base/format_utils.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/platform_features.h"
#include "media/base/video_bitrate_allocation.h"
#include "media/gpu/chromeos/platform_video_frame_utils.h"
#include "media/gpu/gpu_video_encode_accelerator_helpers.h"
#include "media/gpu/h264_dpb.h"
#include "media/gpu/macros.h"
#include "media/gpu/vaapi/av1_vaapi_video_encoder_delegate.h"
#include "media/gpu/vaapi/h264_vaapi_video_encoder_delegate.h"
#include "media/gpu/vaapi/vaapi_common.h"
#include "media/gpu/vaapi/vaapi_utils.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "media/gpu/vaapi/vp8_vaapi_video_encoder_delegate.h"
#include "media/gpu/vaapi/vp9_vaapi_video_encoder_delegate.h"
#include "media/gpu/vp8_reference_frame_vector.h"
#include "media/gpu/vp9_reference_frame_vector.h"

namespace media {

namespace {
// Minimum number of frames in flight for pipeline depth, adjust to this number
// if encoder requests less.
constexpr size_t kMinNumFramesInFlight = 4;

// VASurfaceIDs internal format.
constexpr unsigned int kVaSurfaceFormat = VA_RT_FORMAT_YUV420;

// Creates one |encode_size| ScopedVASurface using |vaapi_wrapper|.
std::unique_ptr<ScopedVASurface> CreateScopedSurface(
    VaapiWrapper& vaapi_wrapper,
    const gfx::Size& encode_size,
    const std::vector<VaapiWrapper::SurfaceUsageHint>& surface_usage_hints) {
  auto surfaces = vaapi_wrapper.CreateScopedVASurfaces(
      kVaSurfaceFormat, encode_size, surface_usage_hints, 1u,
      /*visible_size=*/std::nullopt,
      /*va_fourcc=*/std::nullopt);
  return surfaces.empty() ? nullptr : std::move(surfaces.front());
}

}  // namespace

struct VaapiVideoEncodeAccelerator::InputFrameRef {
  InputFrameRef(scoped_refptr<VideoFrame> frame, bool force_keyframe)
      : frame(frame), force_keyframe(force_keyframe) {}
  // If |frame| is nullptr, the InputFrameRef indicates the Flush request.
  const scoped_refptr<VideoFrame> frame;
  const bool force_keyframe;
};

class VaapiVideoEncodeAccelerator::ScopedVASurfaceWrapper {
 public:
  using ReleaseCB = base::OnceCallback<void(std::unique_ptr<ScopedVASurface>)>;

  ScopedVASurfaceWrapper(std::unique_ptr<ScopedVASurface> surface,
                         ReleaseCB release_cb)
      : surface_(std::move(surface)), release_cb_(std::move(release_cb)) {
    DCHECK(release_cb_);
  }
  ~ScopedVASurfaceWrapper() {
    if (release_cb_) {
      std::move(release_cb_).Run(std::move(surface_));
    }
  }

  ScopedVASurfaceWrapper& operator=(const ScopedVASurfaceWrapper&) = delete;
  ScopedVASurfaceWrapper(const ScopedVASurfaceWrapper&) = delete;

  const ScopedVASurface& surface() const { return *surface_.get(); }

  std::unique_ptr<VASurfaceHandle> ReleaseAsVASurfaceHandle() {
    const auto id = surface_->id();
    return std::make_unique<VASurfaceHandle>(
        id,
        // This lambda is an adapter to ScopedID::ReleaseCB which uses a
        // VASurfaceID parameter.
        base::BindOnce(
            [](std::unique_ptr<ScopedVASurface> surface, ReleaseCB release_cb,
               VASurfaceID /*va_surface_id*/) {
              if (release_cb) {
                std::move(release_cb).Run(std::move(surface));
              }
            },
            std::move(surface_), std::move(release_cb_)));
  }

 private:
  std::unique_ptr<ScopedVASurface> surface_;
  ReleaseCB release_cb_;
};

// static
base::AtomicRefCount VaapiVideoEncodeAccelerator::num_instances_(0);

VideoEncodeAccelerator::SupportedProfiles
VaapiVideoEncodeAccelerator::GetSupportedProfiles() {
  if (IsConfiguredForTesting())
    return supported_profiles_for_testing_;
  return VaapiWrapper::GetSupportedEncodeProfiles();
}

VaapiVideoEncodeAccelerator::VaapiVideoEncodeAccelerator()
    : can_use_encoder_(num_instances_.Increment() < kMaxNumOfInstances),
      child_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      // TODO(akahuang): Change to use SequencedTaskRunner to see if the
      // performance is affected.
      encoder_task_runner_(base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN, base::MayBlock(),
           base::TaskPriority::USER_VISIBLE},
          base::SingleThreadTaskRunnerThreadMode::DEDICATED)) {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(child_sequence_checker_);
  DETACH_FROM_SEQUENCE(encoder_sequence_checker_);

  child_weak_this_ = child_weak_this_factory_.GetWeakPtr();
  encoder_weak_this_ = encoder_weak_this_factory_.GetWeakPtr();

  // The default value of VideoEncoderInfo of VaapiVideoEncodeAccelerator.
  encoder_info_.implementation_name = "VaapiVideoEncodeAccelerator";
  DCHECK(!encoder_info_.has_trusted_rate_controller);
  DCHECK(encoder_info_.is_hardware_accelerated);
  DCHECK(encoder_info_.supports_native_handle);
  DCHECK(!encoder_info_.supports_simulcast);
}

VaapiVideoEncodeAccelerator::~VaapiVideoEncodeAccelerator() {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);

  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);

  num_instances_.Decrement();
}

bool VaapiVideoEncodeAccelerator::Initialize(
    const Config& config,
    Client* client,
    std::unique_ptr<MediaLog> media_log) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(child_sequence_checker_);
  VLOGF(2) << "Initializing VAVEA, " << config.AsHumanReadableString();

  if (!can_use_encoder_) {
    MEDIA_LOG(ERROR, media_log.get()) << "Too many encoders are allocated";
    return false;
  }

  if (AttemptedInitialization()) {
    MEDIA_LOG(ERROR, media_log.get())
        << "Initialize() cannot be called more than once.";
    return false;
  }

  client_ptr_factory_.reset(new base::WeakPtrFactory<Client>(client));
  client_ = client_ptr_factory_->GetWeakPtr();

  if (config.HasSpatialLayer()) {
    if (config.output_profile != VideoCodecProfile::VP9PROFILE_PROFILE0) {
      MEDIA_LOG(ERROR, media_log.get())
          << "Spatial layers are only supported for VP9 encoding";
      return false;
    }

    if (config.inter_layer_pred != SVCInterLayerPredMode::kOnKeyPic &&
        config.inter_layer_pred != SVCInterLayerPredMode::kOff) {
      MEDIA_LOG(ERROR, media_log.get())
          << "Only K-SVC and S mode encoding are supported.";
      return false;
    }

#if BUILDFLAG(IS_CHROMEOS)
    if (!IsConfiguredForTesting()) {
      if (config.inter_layer_pred == SVCInterLayerPredMode::kOff &&
          !base::FeatureList::IsEnabled(kVaapiVp9SModeHWEncoding)) {
        MEDIA_LOG(ERROR, media_log.get()) << "Vp9 S-mode encoding is disabled";
        return false;
      }
    }
#endif  // BUILDFLAG(IS_CHROMEOS)

    // TODO(crbug.com/40172317): Remove this restriction.
    if (!base::ranges::is_sorted(
            config.spatial_layers,
            [](const VideoEncodeAccelerator::Config::SpatialLayer& lhs,
               const VideoEncodeAccelerator::Config::SpatialLayer& rhs) {
              return lhs.width < rhs.width && lhs.height < rhs.height;
            })) {
      MEDIA_LOG(ERROR, media_log.get())
          << "Doesn't support k-SVC encoding where spatial layers "
             "have the same resolution";
      return false;
    }
  }

  const VideoCodec codec = VideoCodecProfileToVideoCodec(config.output_profile);
  if (codec != VideoCodec::kH264 && codec != VideoCodec::kVP8 &&
      codec != VideoCodec::kVP9 && codec != VideoCodec::kAV1) {
    MEDIA_LOG(ERROR, media_log.get())
        << "Unsupported profile: " << GetProfileName(config.output_profile);
    return false;
  }

  if (config.bitrate.mode() == Bitrate::Mode::kVariable) {
    if (!base::FeatureList::IsEnabled(kChromeOSHWVBREncoding)) {
      MEDIA_LOG(ERROR, media_log.get()) << "Variable bitrate is disabled.";
      return false;
    }
    if (codec != VideoCodec::kH264) {
      MEDIA_LOG(ERROR, media_log.get())
          << "Variable bitrate is only supported with H264 encoding.";
      return false;
    }
  }

  if (config.input_format != PIXEL_FORMAT_I420 &&
      config.input_format != PIXEL_FORMAT_NV12) {
    MEDIA_LOG(ERROR, media_log.get())
        << "Unsupported input format: " << config.input_format;
    return false;
  }

  bool native_input_mode =
      config.storage_type == Config::StorageType::kGpuMemoryBuffer;
  if (native_input_mode && config.input_format != PIXEL_FORMAT_NV12) {
    // TODO(crbug.com/894381): Support other formats.
    MEDIA_LOG(ERROR, media_log.get())
        << "Unsupported format for native input mode: "
        << VideoPixelFormatToString(config.input_format);
    return false;
  }

  if (config.HasSpatialLayer() && !native_input_mode) {
    MEDIA_LOG(ERROR, media_log.get())
        << "Spatial scalability is only supported for native input now";
    return false;
  }

  const SupportedProfiles& profiles = GetSupportedProfiles();
  const auto profile = find_if(profiles.begin(), profiles.end(),
                               [output_profile = config.output_profile](
                                   const SupportedProfile& profile) {
                                 return profile.profile == output_profile;
                               });
  if (profile == profiles.end()) {
    MEDIA_LOG(ERROR, media_log.get()) << "Unsupported output profile "
                                      << GetProfileName(config.output_profile);
    return false;
  }

  if (config.input_visible_size.width() > profile->max_resolution.width() ||
      config.input_visible_size.height() > profile->max_resolution.height()) {
    MEDIA_LOG(ERROR, media_log.get())
        << "Input size too big: " << config.input_visible_size.ToString()
        << ", max supported size: " << profile->max_resolution.ToString();
    return false;
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

  native_input_mode_ =
      config.storage_type == Config::StorageType::kGpuMemoryBuffer;

  output_codec_ = VideoCodecProfileToVideoCodec(config.output_profile);
  DCHECK_EQ(IsConfiguredForTesting(), !!vaapi_wrapper_);
  if (!IsConfiguredForTesting()) {
    VaapiWrapper::CodecMode mode;
    switch (output_codec_) {
      case VideoCodec::kH264:
        if (H264VaapiVideoEncoderDelegate::UseSoftwareRateController(config)) {
          mode = VaapiWrapper::kEncodeConstantQuantizationParameter;
        } else {
          mode = config.bitrate.mode() == Bitrate::Mode::kConstant
                     ? VaapiWrapper::kEncodeConstantBitrate
                     : VaapiWrapper::kEncodeVariableBitrate;
        }
        break;
      case VideoCodec::kVP8:
      case VideoCodec::kVP9:
      case VideoCodec::kAV1:
        mode = VaapiWrapper::kEncodeConstantQuantizationParameter;
        break;
      default:
        NotifyError({EncoderStatus::Codes::kEncoderUnsupportedCodec,
                     "Unsupported codec: " + GetCodecName(output_codec_)});
        return;
    }

    vaapi_wrapper_ =
        VaapiWrapper::CreateForVideoCodec(
            mode, config.output_profile, EncryptionScheme::kUnencrypted,
            base::BindRepeating(&ReportVaapiErrorToUMA,
                                "Media.VaapiVideoEncodeAccelerator.VAAPIError"))
            .value_or(nullptr);

    if (!vaapi_wrapper_) {
      NotifyError({EncoderStatus::Codes::kEncoderInitializationError,
                   "Failed initializing VAAPI for profile " +
                       GetProfileName(config.output_profile)});
      return;
    }
  }

  DCHECK_EQ(IsConfiguredForTesting(), !!encoder_);
  // Base::Unretained(this) is safe because |error_cb| is called by
  // |encoder_| and |this| outlives |encoder_|.
  auto error_cb = base::BindRepeating(
      [](VaapiVideoEncodeAccelerator* const vea) {
        // TODO(b/276005687): Report encoder status from
        // VaapiVIdeoEncoderDelegate.
        vea->NotifyError({EncoderStatus::Codes::kEncoderFailedEncode,
                          "VaapiVideoEncodeAcceleratorDelegate error"});
      },
      base::Unretained(this));

  VaapiVideoEncoderDelegate::Config ave_config{};
  switch (output_codec_) {
    case VideoCodec::kH264:
      if (!IsConfiguredForTesting()) {
        encoder_ = std::make_unique<H264VaapiVideoEncoderDelegate>(
            vaapi_wrapper_, error_cb);
        // HW encoders on Intel GPUs will not put average QP in slice/tile
        // header when it is not working at CQP mode. Currently only H264 is
        // working at non CQP mode.
        if (VaapiWrapper::GetImplementationType() ==
                VAImplementation::kIntelI965 ||
            VaapiWrapper::GetImplementationType() ==
                VAImplementation::kIntelIHD) {
          encoder_info_.reports_average_qp = false;
        }
      }
      break;
    case VideoCodec::kVP8:
      if (!IsConfiguredForTesting()) {
        encoder_ = std::make_unique<VP8VaapiVideoEncoderDelegate>(
            vaapi_wrapper_, error_cb);
      }
      break;
    case VideoCodec::kVP9:
      if (!IsConfiguredForTesting()) {
        encoder_ = std::make_unique<VP9VaapiVideoEncoderDelegate>(
            vaapi_wrapper_, error_cb);
      }
      break;
    case VideoCodec::kAV1:
      if (!IsConfiguredForTesting()) {
        encoder_ = std::make_unique<AV1VaapiVideoEncoderDelegate>(
            vaapi_wrapper_, error_cb);
      }
      break;
    default:
      NOTREACHED_IN_MIGRATION()
          << "Unsupported codec type " << GetCodecName(output_codec_);
      return;
  }

  if (!vaapi_wrapper_->GetVAEncMaxNumOfRefFrames(
          config.output_profile, &ave_config.max_num_ref_frames)) {
    NotifyError({EncoderStatus::Codes::kEncoderHardwareDriverError,
                 "Failed getting max number of reference frames supported by "
                 "the driver"});
    return;
  }
  DCHECK_GT(ave_config.max_num_ref_frames, 0u);
  if (!encoder_->Initialize(config, ave_config)) {
    NotifyError({EncoderStatus::Codes::kEncoderInitializationError,
                 base::StrCat({"Failed initializing encoder. config: ",
                               config.AsHumanReadableString()})});
    return;
  }

  output_buffer_byte_size_ = encoder_->GetBitstreamBufferSize();

  visible_rect_ = gfx::Rect(config.input_visible_size);
  expected_input_coded_size_ = VideoFrame::DetermineAlignedSize(
      config.input_format, config.input_visible_size);
  DCHECK(
      expected_input_coded_size_.width() <= encoder_->GetCodedSize().width() &&
      expected_input_coded_size_.height() <= encoder_->GetCodedSize().height());

  // The number of required buffers is the number of required reference frames
  // + 1 for the current frame to be encoded.
  const size_t max_ref_frames = encoder_->GetMaxNumOfRefFrames();
  num_frames_in_flight_ = std::max(kMinNumFramesInFlight, max_ref_frames);
  DVLOGF(1) << "Frames in flight: " << num_frames_in_flight_;
  max_pending_results_size_ =
      num_frames_in_flight_ * std::max<size_t>(1, config.spatial_layers.size());
  if (!vaapi_wrapper_->CreateContext(encoder_->GetCodedSize())) {
    NotifyError({EncoderStatus::Codes::kEncoderInitializationError,
                 base::StrCat({"Failed creating VAContext. config: ",
                               config.AsHumanReadableString()})});
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
      encoder_info_.fps_allocation[i] =
          GetFpsAllocation(config.spatial_layers[i].num_of_temporal_layers);
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

  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "media::VaapiVideoEncodeAccelerator", encoder_task_runner_);
}

void VaapiVideoEncodeAccelerator::RecycleInputScopedVASurface(
    const gfx::Size& encode_size,
    std::unique_ptr<ScopedVASurface> va_surface) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  DCHECK(va_surface);
  DVLOGF(4) << "va_surface->id()=" << va_surface->id();
  input_surfaces_[encode_size] = std::move(va_surface);
}

void VaapiVideoEncodeAccelerator::RecycleEncodeScopedVASurface(
    const gfx::Size& encode_size,
    std::unique_ptr<ScopedVASurface> va_surface) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  DCHECK(va_surface);
  DVLOGF(4) << "va_surface->id()=" << va_surface->id();
  available_encode_surfaces_[encode_size].push_back(std::move(va_surface));
}

void VaapiVideoEncodeAccelerator::TryToReturnBitstreamBuffers() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);

  if (state_ != kEncoding)
    return;

  TRACE_EVENT2("media,gpu", "VAVEA::TryToReturnBitstreamBuffers",
               "pending encode results", pending_encode_results_.size(),
               "available bitstream buffers",
               available_bitstream_buffers_.size());
  while (!pending_encode_results_.empty()) {
    if (!pending_encode_results_.front()) {
      // A null job indicates a flush command.
      pending_encode_results_.pop();
      DVLOGF(2) << "FlushDone";
      DCHECK(flush_callback_);
      child_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(std::move(flush_callback_), true));
      continue;
    }

    if (available_bitstream_buffers_.empty())
      return;

    ReturnBitstreamBuffer(*pending_encode_results_.front(),
                          available_bitstream_buffers_.front());
    available_bitstream_buffers_.pop();
    pending_encode_results_.pop();
  }
}

void VaapiVideoEncodeAccelerator::ReturnBitstreamBuffer(
    const EncodeResult& encode_result,
    const BitstreamBuffer& buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);

  auto metadata = encode_result.metadata();

  if (!encode_result.IsFrameDropped()) {
    const base::UnsafeSharedMemoryRegion& shm_region = buffer.region();
    DCHECK(shm_region.IsValid());
    base::WritableSharedMemoryMapping shm_mapping = shm_region.Map();
    uint8_t* target_data = shm_mapping.GetMemoryAs<uint8_t>();
    size_t data_size = 0;
    // vaSyncSurface() is not necessary because GetEncodedChunkSize() has been
    // called in VaapiVideoEncoderDelegate::Encode().
    if (!vaapi_wrapper_->DownloadFromVABuffer(
            encode_result.coded_buffer_id(), /*sync_surface_id=*/std::nullopt,
            target_data, shm_mapping.size(), &data_size)) {
      NotifyError({EncoderStatus::Codes::kEncoderHardwareDriverError,
                   "Failed downloading coded buffer"});
      return;
    }
    CHECK_EQ(metadata.payload_size_bytes, data_size);
    DVLOGF(4) << "Returning bitstream buffer "
              << (metadata.key_frame ? "(keyframe)" : "")
              << " id: " << buffer.id() << " size: " << data_size;
  } else {
    CHECK(metadata.dropped_frame());
    CHECK_EQ(metadata.payload_size_bytes, 0u);
    DVLOGF(4) << "Drop frame bitstream_buffer_id=" << buffer.id();
  }

  TRACE_EVENT2("media,gpu", "VAVEA::BitstreamBufferReady", "timestamp",
               metadata.timestamp.InMicroseconds(), "bitstream_buffer_id",
               buffer.id());
  child_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Client::BitstreamBufferReady, client_,
                                buffer.id(), std::move(metadata)));
}

void VaapiVideoEncodeAccelerator::Encode(scoped_refptr<VideoFrame> frame,
                                         bool force_keyframe) {
  DVLOGF(4) << "Frame timestamp: " << frame->timestamp().InMicroseconds()
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
  if (frame) {
    TRACE_EVENT1("media,gpu", "VAVEA::EncodeTask", "timestamp",
                 frame->timestamp().InMicroseconds());
    // |frame| can be nullptr to indicate a flush.
    const bool is_expected_storage_type =
        native_input_mode_
            ? frame->storage_type() == VideoFrame::STORAGE_GPU_MEMORY_BUFFER
            : frame->IsMappable();
    if (!is_expected_storage_type) {
      NotifyError({EncoderStatus::Codes::kInvalidInputFrame,
                   "Unexpected storage: " +
                       VideoFrame::StorageTypeToString(frame->storage_type())});
      return;
    }
  }

  input_queue_.emplace(std::move(frame), force_keyframe);
  EncodePendingInputs();
}

bool VaapiVideoEncodeAccelerator::CreateSurfacesForGpuMemoryBufferEncoding(
    const VideoFrame& frame,
    const std::vector<gfx::Size>& spatial_layer_resolutions,
    std::vector<std::unique_ptr<ScopedVASurfaceWrapper>>* input_surfaces,
    std::vector<std::unique_ptr<ScopedVASurfaceWrapper>>*
        reconstructed_surfaces) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  DCHECK(native_input_mode_);
  DCHECK_EQ(frame.storage_type(), VideoFrame::STORAGE_GPU_MEMORY_BUFFER);
  TRACE_EVENT0("media,gpu", "VAVEA::CreateSurfacesForGpuMemoryBuffer");

  if (frame.format() != PIXEL_FORMAT_NV12) {
    NotifyError(
        {EncoderStatus::Codes::kUnsupportedFrameFormat,
         "Expected NV12, got: " + VideoPixelFormatToString(frame.format())});
    return false;
  }

  if (spatial_layer_resolutions.empty())
    return false;

  for (const auto& encode_size : spatial_layer_resolutions) {
    reconstructed_surfaces->push_back(
        GetOrCreateReconstructedSurface(encode_size));
    if (!reconstructed_surfaces->back()) {
      return false;
    }
  }

  std::unique_ptr<ScopedVASurface> source_surface;
  {
    TRACE_EVENT0("media,gpu", "VAVEA::ImportGpuMemoryBufferToVASurface");

    // Create VASurface from GpuMemory-based VideoFrame.
    scoped_refptr<gfx::NativePixmap> pixmap = CreateNativePixmapDmaBuf(&frame);
    if (!pixmap) {
      NotifyError({EncoderStatus::Codes::kSystemAPICallError,
                   "Failed to create NativePixmap from VideoFrame"});
      return false;
    }

    source_surface =
        vaapi_wrapper_->CreateVASurfaceForPixmap(std::move(pixmap));
    if (!source_surface) {
      NotifyError({EncoderStatus::Codes::kEncoderHardwareDriverError,
                   "Failed to create VASurface"});
      return false;
    }
  }

  // The downscaling for-loop below relies on |spatial_layer_resolutions|
  // ordered from small to larger ones. It cannot contain duplicates.
  // TODO(crbug.com/40172317): Consider supporting multiple layers with the
  // same resolution.
  CHECK(base::ranges::is_sorted(spatial_layer_resolutions,
                                [](const gfx::Size& lhs, const gfx::Size& rhs) {
                                  return lhs.width() < rhs.width() &&
                                         lhs.height() < rhs.height();
                                }));

  // Create input surfaces.
  TRACE_EVENT1("media,gpu", "VAVEA::ConstructSurfaces", "layers",
               spatial_layer_resolutions.size());
  auto source_rect = frame.visible_rect();
  for (const gfx::Size& encode_size : spatial_layer_resolutions) {
    const bool engage_vpp = source_rect != gfx::Rect(encode_size);
    // Crop and scale |source_surface| to a surface whose size is |encode_size|.
    // The size of a reconstructed surface is also |encode_size|.
    CHECK(source_surface);
    if (engage_vpp) {
      input_surfaces->push_back(
          ExecuteBlitSurface(source_surface.get(), source_rect, encode_size));
    } else {
      input_surfaces->push_back(std::make_unique<ScopedVASurfaceWrapper>(
          std::move(source_surface), base::DoNothing()));
    }

    if (!input_surfaces->back()) {
      return false;
    }
  }

  return true;
}

bool VaapiVideoEncodeAccelerator::CreateSurfacesForShmemEncoding(
    const VideoFrame& frame,
    std::unique_ptr<ScopedVASurfaceWrapper>* input_surface,
    std::unique_ptr<ScopedVASurfaceWrapper>* reconstructed_surface) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  DCHECK(!native_input_mode_);
  DCHECK(frame.IsMappable());
  TRACE_EVENT0("media,gpu", "VAVEA::CreateSurfacesForShmem");

  if (expected_input_coded_size_ != frame.coded_size()) {
    // In non-zero copy mode, the coded size of the incoming frame should be
    // the same as the one we requested through
    // Client::RequireBitstreamBuffers().
    NotifyError(
        {EncoderStatus::Codes::kInvalidInputFrame,
         "Expected frame coded size: " + expected_input_coded_size_.ToString() +
             ", but got: " + frame.coded_size().ToString()});
    return false;
  }

  DCHECK(visible_rect_.origin().IsOrigin());
  if (visible_rect_ != frame.visible_rect()) {
    // In non-zero copy mode, the client is responsible for scaling and
    // cropping.
    NotifyError(
        {EncoderStatus::Codes::kInvalidInputFrame,
         "Expected frame visible rectangle: " + visible_rect_.ToString() +
             ", but got: " + frame.visible_rect().ToString()});
    return false;
  }

  const gfx::Size& encode_size = encoder_->GetCodedSize();
  *reconstructed_surface = GetOrCreateReconstructedSurface(encode_size);
  if (!*reconstructed_surface) {
    return false;
  }

  *input_surface =
      GetOrCreateInputSurface(*vaapi_wrapper_, encode_size,
                              {VaapiWrapper::SurfaceUsageHint::kVideoEncoder});
  if (!*input_surface) {
    NotifyError({EncoderStatus::Codes::kEncoderIllegalState,
                 "Failed to create input surface"});
    return false;
  }

  if (!vaapi_wrapper_->UploadVideoFrameToSurface(
          frame, (*input_surface)->surface().id(),
          (*input_surface)->surface().size())) {
    NotifyError({EncoderStatus::Codes::kEncoderHardwareDriverError,
                 "Failed to upload frame"});
    return false;
  }

  return true;
}

std::unique_ptr<VaapiVideoEncodeAccelerator::ScopedVASurfaceWrapper>
VaapiVideoEncodeAccelerator::GetOrCreateInputSurface(
    VaapiWrapper& vaapi_wrapper,
    const gfx::Size& encode_size,
    const std::vector<VaapiWrapper::SurfaceUsageHint>& surface_usage_hints) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  if (!base::Contains(input_surfaces_, encode_size)) {
    auto surface =
        CreateScopedSurface(vaapi_wrapper, encode_size, surface_usage_hints);
    if (!surface) {
      NotifyError({EncoderStatus::Codes::kEncoderHardwareDriverError,
                   "Failed to create surface"});
      return nullptr;
    }

    input_surfaces_[encode_size] = std::move(surface);
  }

  auto surface_and_cb = std::make_unique<ScopedVASurfaceWrapper>(
      std::move(input_surfaces_[encode_size]),
      base::BindOnce(&VaapiVideoEncodeAccelerator::RecycleInputScopedVASurface,
                     encoder_weak_this_, encode_size));

  input_surfaces_.erase(encode_size);
  return surface_and_cb;
}

std::unique_ptr<VaapiVideoEncodeAccelerator::ScopedVASurfaceWrapper>
VaapiVideoEncodeAccelerator::GetOrCreateReconstructedSurface(
    const gfx::Size& encode_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  const size_t max_allocated_surfaces = num_frames_in_flight_ + 1;
  const bool no_surfaces_available =
      !base::Contains(available_encode_surfaces_, encode_size) ||
      available_encode_surfaces_[encode_size].empty();
  if (no_surfaces_available &&
      encode_surfaces_count_[encode_size] >= max_allocated_surfaces) {
    DVLOGF(4) << "Not enough surfaces available";
    return nullptr;
  }

  if (no_surfaces_available) {
    auto surface =
        CreateScopedSurface(*vaapi_wrapper_, encode_size,
                            {VaapiWrapper::SurfaceUsageHint::kVideoEncoder});
    if (!surface) {
      NotifyError({EncoderStatus::Codes::kEncoderHardwareDriverError,
                   "Failed creating surfaces"});
      return nullptr;
    }

    available_encode_surfaces_[encode_size].push_back(std::move(surface));
    encode_surfaces_count_[encode_size] += 1;
  }

  auto surface_and_cb = std::make_unique<ScopedVASurfaceWrapper>(
      std::move(available_encode_surfaces_[encode_size].back()),
      base::BindOnce(&VaapiVideoEncodeAccelerator::RecycleEncodeScopedVASurface,
                     encoder_weak_this_, encode_size));

  available_encode_surfaces_[encode_size].pop_back();
  return surface_and_cb;
}

scoped_refptr<VaapiWrapper>
VaapiVideoEncodeAccelerator::CreateVppVaapiWrapper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  DCHECK(!vpp_vaapi_wrapper_);
  auto vpp_vaapi_wrapper =
      VaapiWrapper::Create(
          VaapiWrapper::kVideoProcess, VAProfileNone,
          EncryptionScheme::kUnencrypted,
          base::BindRepeating(
              &ReportVaapiErrorToUMA,
              "Media.VaapiVideoEncodeAccelerator.Vpp.VAAPIError"))
          .value_or(nullptr);
  if (!vpp_vaapi_wrapper) {
    NotifyError({EncoderStatus::Codes::kEncoderUnsupportedConfig,
                 "Failed to initialize VppVaapiWrapper"});
    return nullptr;
  }
  // VA context for VPP is not associated with a specific resolution.
  if (!vpp_vaapi_wrapper->CreateContext(gfx::Size())) {
    NotifyError({EncoderStatus::Codes::kEncoderHardwareDriverError,
                 "Failed creating Context for VPP"});
    return nullptr;
  }

  return vpp_vaapi_wrapper;
}

std::unique_ptr<VaapiVideoEncodeAccelerator::ScopedVASurfaceWrapper>
VaapiVideoEncodeAccelerator::ExecuteBlitSurface(
    const ScopedVASurface* source_surface,
    const gfx::Rect source_visible_rect,
    const gfx::Size& encode_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  if (!vpp_vaapi_wrapper_) {
    vpp_vaapi_wrapper_ = CreateVppVaapiWrapper();
    if (!vpp_vaapi_wrapper_) {
      LOG(ERROR) << "Failed to create Vpp";
      return nullptr;
    }
  }

  auto blit_surface = GetOrCreateInputSurface(
      *vpp_vaapi_wrapper_, encode_size,
      {VaapiWrapper::SurfaceUsageHint::kVideoProcessWrite,
       VaapiWrapper::SurfaceUsageHint::kVideoEncoder});
  if (!blit_surface)
    return nullptr;

  DCHECK(vpp_vaapi_wrapper_);
  TRACE_EVENT2("media,gpu", "VAVEA::ImageProcessor::BlitSurface",
               "source_visible_rect", source_visible_rect.ToString(),
               "dest_visible_rect", gfx::Rect(encode_size).ToString());
  if (!vpp_vaapi_wrapper_->BlitSurface(
          source_surface->id(), source_surface->size(),
          blit_surface->surface().id(), blit_surface->surface().size(),
          source_visible_rect, gfx::Rect(encode_size))) {
    NotifyError({EncoderStatus::Codes::kFormatConversionError,
                 "Failed BlitSurface on frame size: " +
                     source_surface->size().ToString() +
                     " (visible rect: " + source_visible_rect.ToString() +
                     ") -> encode size: " + encode_size.ToString()});
    return nullptr;
  }

  return blit_surface;
}

std::unique_ptr<VaapiVideoEncoderDelegate::EncodeJob>
VaapiVideoEncodeAccelerator::CreateEncodeJob(
    bool force_keyframe,
    base::TimeDelta frame_timestamp,
    uint8_t spatial_index,
    bool end_of_picture,
    VASurfaceID input_surface_id,
    std::unique_ptr<ScopedVASurfaceWrapper> reconstructed_surface) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  DCHECK_NE(input_surface_id, VA_INVALID_ID);

  std::unique_ptr<ScopedVABuffer> coded_buffer;
  {
    TRACE_EVENT1("media,gpu", "VAVEA::CreateVABuffer", "buffer size",
                 output_buffer_byte_size_);
    coded_buffer = vaapi_wrapper_->CreateVABuffer(VAEncCodedBufferType,
                                                  output_buffer_byte_size_);
    if (!coded_buffer) {
      NotifyError({EncoderStatus::Codes::kEncoderHardwareDriverError,
                   "Failed creating coded buffer"});
      return nullptr;
    }
  }

  scoped_refptr<CodecPicture> picture;
  switch (output_codec_) {
    case VideoCodec::kH264:
      picture = new VaapiH264Picture(
          reconstructed_surface->ReleaseAsVASurfaceHandle());
      break;
    case VideoCodec::kVP8:
      picture = new VaapiVP8Picture(
          reconstructed_surface->ReleaseAsVASurfaceHandle());
      break;
    case VideoCodec::kVP9:
      picture = new VaapiVP9Picture(
          reconstructed_surface->ReleaseAsVASurfaceHandle());
      break;
    case VideoCodec::kAV1:
      picture = new VaapiAV1Picture(
          /*display_va_surface=*/nullptr,
          reconstructed_surface->ReleaseAsVASurfaceHandle());
      break;
    default:
      return nullptr;
  }

  return std::make_unique<EncodeJob>(
      force_keyframe, frame_timestamp, spatial_index, end_of_picture,
      input_surface_id, std::move(picture), std::move(coded_buffer));
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

  TRACE_EVENT1("media,gpu", "VAVEA::EncodePendingInputs",
               "pending input frames", input_queue_.size());
  // Encode all the frames in |input_queue_|. So that we avoid a number of
  // encoded chunks are stuck at |pending_encode_results_|, we breaks if the
  // queue size is more than |max_pending_encode_results_size|. Since the
  // pending frames to be encoded are held in |input_queue_|, a client that
  // recycles the VideoFrames will not input any more frames until an available
  // bitstream buffer is given and a pending frame is released thanks to
  // the resumed encode.
  while (state_ == kEncoding && !input_queue_.empty() &&
         pending_encode_results_.size() < max_pending_results_size_) {
    const InputFrameRef& input_frame = input_queue_.front();
    if (!input_frame.frame) {
      // If this is a flush (null) frame, don't create/submit a new encode
      // result for it, but forward std::nulloptto the
      // |pending_encode_results_| queue.
      pending_encode_results_.push(std::nullopt);
      input_queue_.pop();
      TryToReturnBitstreamBuffers();
      continue;
    }

    TRACE_EVENT0("media,gpu",
                 "VAVEA::EncodeOneInputFrameAndReturnEncodedChunks");
    const size_t num_spatial_layers = spatial_layer_resolutions.size();
    std::vector<std::unique_ptr<ScopedVASurfaceWrapper>> input_surfaces;
    std::vector<std::unique_ptr<ScopedVASurfaceWrapper>> reconstructed_surfaces;
    if (native_input_mode_) {
      if (!CreateSurfacesForGpuMemoryBufferEncoding(
              *input_frame.frame, spatial_layer_resolutions, &input_surfaces,
              &reconstructed_surfaces)) {
        return;
      }
    } else {
      DCHECK_EQ(num_spatial_layers, 1u);
      input_surfaces.resize(1u);
      reconstructed_surfaces.resize(1u);
      if (!CreateSurfacesForShmemEncoding(*input_frame.frame,
                                          &input_surfaces[0],
                                          &reconstructed_surfaces[0])) {
        return;
      }
    }
    CHECK_EQ(num_spatial_layers, input_surfaces.size());
    CHECK_EQ(num_spatial_layers, reconstructed_surfaces.size());

    // Encoding different spatial layers for |input_frame|.
    std::vector<std::unique_ptr<EncodeJob>> jobs;
    for (size_t spatial_idx = 0; spatial_idx < num_spatial_layers;
         ++spatial_idx) {
      TRACE_EVENT0("media,gpu", "VAVEA::CreateEncoderJob");
      const bool force_key =
          (spatial_idx == 0 ? input_frame.force_keyframe : false);
      const bool end_of_picture = spatial_idx == num_spatial_layers - 1;
      std::unique_ptr<EncodeJob> job = CreateEncodeJob(
          force_key, input_frame.frame->timestamp(),
          base::checked_cast<uint8_t>(spatial_idx), end_of_picture,
          input_surfaces[spatial_idx]->surface().id(),
          std::move(reconstructed_surfaces[spatial_idx]));
      if (!job)
        return;

      jobs.emplace_back(std::move(job));
    }
    for (auto& job : jobs) {
      TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("media,gpu", "PlatformEncoding.Encode",
                                        TRACE_ID_LOCAL(&job));

      if (!encoder_->Encode(*job)) {
        NotifyError({EncoderStatus::Codes::kEncoderFailedEncode,
                     "Failed encoding job"});
        return;
      }
    }
    for (size_t i = 0; i < jobs.size(); i++) {
      std::optional<EncodeResult> result =
          encoder_->GetEncodeResult(std::move(jobs[i]));
      if (!result) {
        NotifyError({EncoderStatus::Codes::kEncoderFailedEncode,
                     "Failed getting encode result"});
        return;
      }

      TRACE_EVENT_NESTABLE_ASYNC_END2(
          "media,gpu", "PlatformEncoding.Encode", TRACE_ID_LOCAL(&jobs[i]),
          "timestamp", result->metadata().timestamp.InMicroseconds(), "size",
          spatial_layer_resolutions[i].ToString());

      pending_encode_results_.push(std::move(result));
    }

    // Invalidates |input_frame| here; it notifies a client |input_frame.frame|
    // can be reused for the future encoding.
    // If the frame is copied (|native_input_mode_| == false), it is clearly
    // safe to release |input_frame|. If the frame is imported
    // (|native_input_mode_| == true), the write operation to the frame is
    // blocked on DMA_BUF_IOCTL_SYNC because a VA-API driver protects the buffer
    // through a DRM driver until encoding is complete, that is, vaMapBuffer()
    // on a coded buffer returns.
    input_queue_.pop();

    TryToReturnBitstreamBuffers();
  }
}

void VaapiVideoEncodeAccelerator::UseOutputBitstreamBuffer(
    BitstreamBuffer buffer) {
  DVLOGF(4) << "id: " << buffer.id();
  DCHECK_CALLED_ON_VALID_SEQUENCE(child_sequence_checker_);

  encoder_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VaapiVideoEncodeAccelerator::UseOutputBitstreamBufferTask,
                     encoder_weak_this_, std::move(buffer)));
}

void VaapiVideoEncodeAccelerator::UseOutputBitstreamBufferTask(
    BitstreamBuffer buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  DCHECK_NE(state_, kUninitialized);

  if (buffer.size() < output_buffer_byte_size_) {
    NotifyError({EncoderStatus::Codes::kInvalidOutputBuffer,
                 "Provided bitstream buffer too small"});
    return;
  }

  available_bitstream_buffers_.push(std::move(buffer));
  TryToReturnBitstreamBuffers();
  // If there is a pending frame, it is pended because of the bitstream buffer
  // shortage. Try to encode it.
  if (!input_queue_.empty()) {
    EncodePendingInputs();
  }
}

void VaapiVideoEncodeAccelerator::RequestEncodingParametersChange(
    const Bitrate& bitrate,
    uint32_t framerate,
    const std::optional<gfx::Size>& size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(child_sequence_checker_);

  VideoBitrateAllocation allocation(bitrate.mode());
  allocation.SetBitrate(0, 0, bitrate.target_bps());
  allocation.SetPeakBps(bitrate.peak_bps());
  encoder_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &VaapiVideoEncodeAccelerator::RequestEncodingParametersChangeTask,
          encoder_weak_this_, allocation, framerate, size));
}

void VaapiVideoEncodeAccelerator::RequestEncodingParametersChange(
    const VideoBitrateAllocation& bitrate_allocation,
    uint32_t framerate,
    const std::optional<gfx::Size>& size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(child_sequence_checker_);

  encoder_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &VaapiVideoEncodeAccelerator::RequestEncodingParametersChangeTask,
          encoder_weak_this_, bitrate_allocation, framerate, size));
}

void VaapiVideoEncodeAccelerator::RequestEncodingParametersChangeTask(
    VideoBitrateAllocation bitrate_allocation,
    uint32_t framerate,
    const std::optional<gfx::Size>& size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  DCHECK_NE(state_, kUninitialized);

  if (size.has_value()) {
    NotifyError({EncoderStatus::Codes::kEncoderUnsupportedConfig,
                 "Update output frame size is not supported"});
    return;
  }
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
    NotifyError({EncoderStatus::Codes::kEncoderIllegalState,
                 "There is a pending flush"});
    child_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(flush_callback), false));
    return;
  }
  flush_callback_ = std::move(flush_callback);

  // Insert InputFrameRef whose frame is nullptr to indicate a flush command.
  input_queue_.emplace(nullptr, false);
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
  if (client_ptr_factory_)
    client_ptr_factory_->InvalidateWeakPtrs();

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

  if (vpp_vaapi_wrapper_)
    vpp_vaapi_wrapper_->DestroyContext();

  input_surfaces_.clear();

  available_bitstream_buffers_ = {};
  input_queue_ = {};

  // Note ScopedVABuffer owned by EncodeResults must be destroyed before
  // |vaapi_wrapper_| is destroyed to ensure VADisplay is valid on the
  // ScopedVABuffer's destruction.
  DCHECK(vaapi_wrapper_ || pending_encode_results_.empty());
  pending_encode_results_ = {};

  encoder_.reset();

  // Clear |available_encode_surfaces_| after |encoder_| is destroyed because
  // the reconstructed surface in the reference frame pool owned by |encoder_|
  // are back to |available_encode_surfaces_|.
  available_encode_surfaces_.clear();

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

  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  if (VLOG_IS_ON(2)) {
    constexpr auto kStateToString = base::MakeFixedFlatMap<State, const char*>(
        {{kUninitialized, "kUninitialized"},
         {kEncoding, "kEncoding"},
         {kError, "kError"}});
    CHECK(base::Contains(kStateToString, state));
    VLOGF(2) << "setting state to: " << kStateToString.at(state);
  }

  state_ = state;
}

void VaapiVideoEncodeAccelerator::NotifyError(EncoderStatus status) {
  if (!child_task_runner_->RunsTasksInCurrentSequence()) {
    child_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&VaapiVideoEncodeAccelerator::NotifyError,
                                  child_weak_this_, std::move(status)));
    return;
  }

  SetState(kError);
  CHECK(!status.is_ok());
  LOG(ERROR) << "Calling NotifyErrorStatus(" << static_cast<int>(status.code())
             << "), message=" << status.message();
  if (client_) {
    client_->NotifyErrorStatus(status);
    client_ptr_factory_->InvalidateWeakPtrs();
  }
}

bool VaapiVideoEncodeAccelerator::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  using base::trace_event::MemoryAllocatorDump;
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  auto dump_name = base::StringPrintf("gpu/vaapi/encoder/0x%" PRIxPTR,
                                      reinterpret_cast<uintptr_t>(this));

  MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(dump_name);
  dump->AddString("encoder native input mode", "",
                  native_input_mode_ ? "true" : "false");

  constexpr double kNumBytesPerPixelYUV420 = 12.0 / 8;

  for (const auto& surface : encode_surfaces_count_) {
    const gfx::Size& resolution = surface.first;
    const size_t count = surface.second;
    MemoryAllocatorDump* sub_dump = pmd->CreateAllocatorDump(
        dump_name + "/encode surface/" + resolution.ToString());
    sub_dump->AddScalar(MemoryAllocatorDump::kNameObjectCount,
                        MemoryAllocatorDump::kUnitsObjects,
                        static_cast<uint64_t>(count));

    const uint64_t surfaces_packed_size = static_cast<uint64_t>(
        resolution.GetArea() * kNumBytesPerPixelYUV420 * count);
    sub_dump->AddScalar(MemoryAllocatorDump::kNameSize,
                        MemoryAllocatorDump::kUnitsBytes, surfaces_packed_size);
  }

  for (const auto& surface : input_surfaces_) {
    const gfx::Size& resolution = surface.first;
    MemoryAllocatorDump* sub_dump = pmd->CreateAllocatorDump(
        dump_name + "/input surface/" + resolution.ToString());

    const uint64_t surfaces_packed_size =
        static_cast<uint64_t>(resolution.GetArea() * kNumBytesPerPixelYUV420);
    sub_dump->AddScalar(MemoryAllocatorDump::kNameSize,
                        MemoryAllocatorDump::kUnitsBytes, surfaces_packed_size);
  }

  return true;
}
}  // namespace media

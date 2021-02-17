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
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/stl_util.h"
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
#include "media/gpu/vaapi/vaapi_common.h"
#include "media/gpu/vaapi/vaapi_utils.h"
#include "media/gpu/vaapi/vp8_encoder.h"
#include "media/gpu/vaapi/vp9_encoder.h"
#include "media/gpu/vaapi/vp9_temporal_layers.h"
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

// Percentage of bitrate set to be targeted by the HW encoder.
constexpr unsigned int kTargetBitratePercentage = 90;

// Calculate the size of the allocated buffer aligned to hardware/driver
// requirements.
gfx::Size GetInputFrameSize(VideoPixelFormat format,
                            const gfx::Size& visible_size) {
  // Get a VideoFrameLayout of a graphic buffer with the same gfx::BufferUsage
  // as camera stack.
  base::Optional<VideoFrameLayout> layout = GetPlatformVideoFrameLayout(
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

// Encode job for one frame. Created when an input frame is awaiting and
// enough resources are available to proceed. Once the job is prepared and
// submitted to the hardware, it awaits on the |submitted_encode_jobs_| queue
// for an output bitstream buffer to become available. Once one is ready,
// the encoded bytes are downloaded to it, job resources are released
// and become available for reuse.
class VaapiEncodeJob : public AcceleratedVideoEncoder::EncodeJob {
 public:
  VaapiEncodeJob(scoped_refptr<VideoFrame> input_frame,
                 bool keyframe,
                 base::OnceClosure execute_cb,
                 scoped_refptr<VASurface> input_surface,
                 scoped_refptr<CodecPicture> picture,
                 std::unique_ptr<ScopedVABuffer> coded_buffer);

  ~VaapiEncodeJob() override = default;

  VaapiEncodeJob* AsVaapiEncodeJob() override { return this; }

  VABufferID coded_buffer_id() const { return coded_buffer_->id(); }
  const scoped_refptr<VASurface> input_surface() const {
    return input_surface_;
  }
  const scoped_refptr<CodecPicture> picture() const { return picture_; }

 private:
  // Input surface for video frame data or scaled data.
  const scoped_refptr<VASurface> input_surface_;

  const scoped_refptr<CodecPicture> picture_;

  // Buffer that will contain the output bitstream data for this frame.
  const std::unique_ptr<ScopedVABuffer> coded_buffer_;

  DISALLOW_COPY_AND_ASSIGN(VaapiEncodeJob);
};

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

class VaapiVideoEncodeAccelerator::H264Accelerator
    : public H264Encoder::Accelerator {
 public:
  explicit H264Accelerator(VaapiVideoEncodeAccelerator* vea) : vea_(vea) {}

  ~H264Accelerator() override = default;

  // H264Encoder::Accelerator implementation.
  scoped_refptr<H264Picture> GetPicture(
      AcceleratedVideoEncoder::EncodeJob* job) override;

  bool SubmitPackedHeaders(
      AcceleratedVideoEncoder::EncodeJob* job,
      scoped_refptr<H264BitstreamBuffer> packed_sps,
      scoped_refptr<H264BitstreamBuffer> packed_pps) override;

  bool SubmitFrameParameters(
      AcceleratedVideoEncoder::EncodeJob* job,
      const H264Encoder::EncodeParams& encode_params,
      const H264SPS& sps,
      const H264PPS& pps,
      scoped_refptr<H264Picture> pic,
      const std::list<scoped_refptr<H264Picture>>& ref_pic_list0,
      const std::list<scoped_refptr<H264Picture>>& ref_pic_list1) override;

 private:
  VaapiVideoEncodeAccelerator* const vea_;
};

class VaapiVideoEncodeAccelerator::VP8Accelerator
    : public VP8Encoder::Accelerator {
 public:
  explicit VP8Accelerator(VaapiVideoEncodeAccelerator* vea) : vea_(vea) {}

  ~VP8Accelerator() override = default;

  // VP8Encoder::Accelerator implementation.
  scoped_refptr<VP8Picture> GetPicture(
      AcceleratedVideoEncoder::EncodeJob* job) override;

  bool SubmitFrameParameters(AcceleratedVideoEncoder::EncodeJob* job,
                             const VP8Encoder::EncodeParams& encode_params,
                             scoped_refptr<VP8Picture> pic,
                             const Vp8ReferenceFrameVector& ref_frames,
                             const std::array<bool, kNumVp8ReferenceBuffers>&
                                 ref_frames_used) override;

 private:
  VaapiVideoEncodeAccelerator* const vea_;
};

class VaapiVideoEncodeAccelerator::VP9Accelerator
    : public VP9Encoder::Accelerator {
 public:
  explicit VP9Accelerator(VaapiVideoEncodeAccelerator* vea) : vea_(vea) {}

  ~VP9Accelerator() override = default;

  // VP9Encoder::Accelerator implementation.
  scoped_refptr<VP9Picture> GetPicture(
      AcceleratedVideoEncoder::EncodeJob* job) override;

  bool SubmitFrameParameters(
      AcceleratedVideoEncoder::EncodeJob* job,
      const VP9Encoder::EncodeParams& encode_params,
      scoped_refptr<VP9Picture> pic,
      const Vp9ReferenceFrameVector& ref_frames,
      const std::array<bool, kVp9NumRefsPerFrame>& ref_frames_used) override;

 private:
  VaapiVideoEncodeAccelerator* const vea_;
};

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

CodecPicture* VaapiVideoEncodeAccelerator::GetPictureFromJobForTesting(
    VaapiEncodeJob* job) {
  return job->picture().get();
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
  AcceleratedVideoEncoder::Config ave_config{};
  DCHECK_EQ(IsConfiguredForTesting(), !!encoder_);
  switch (output_codec_) {
    case kCodecH264:
      if (!IsConfiguredForTesting()) {
        encoder_ = std::make_unique<H264Encoder>(
            std::make_unique<H264Accelerator>(this));
      }
      DCHECK_EQ(ave_config.bitrate_control,
                AcceleratedVideoEncoder::BitrateControl::kConstantBitrate);
      break;
    case kCodecVP8:
      if (!IsConfiguredForTesting()) {
        encoder_ = std::make_unique<VP8Encoder>(
            std::make_unique<VP8Accelerator>(this));
      }
      DCHECK_EQ(ave_config.bitrate_control,
                AcceleratedVideoEncoder::BitrateControl::kConstantBitrate);
      break;
    case kCodecVP9:
      if (!IsConfiguredForTesting()) {
        encoder_ = std::make_unique<VP9Encoder>(
            std::make_unique<VP9Accelerator>(this));
      }
      ave_config.bitrate_control = AcceleratedVideoEncoder::BitrateControl::
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

  // TODO(crbug.com/1034686): Set ScalingSettings causes getStats() hangs.
  // Investigate and fix the issue.
  // encoder_info_.scaling_settings = encoder_->GetScalingSettings();

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

void VaapiVideoEncodeAccelerator::SubmitBuffer(
    VABufferType type,
    scoped_refptr<base::RefCountedBytes> buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);

  if (!vaapi_wrapper_->SubmitBuffer(type, buffer->size(), buffer->front()))
    NOTIFY_ERROR(kPlatformFailureError, "Failed submitting a buffer");
}

void VaapiVideoEncodeAccelerator::SubmitVAEncMiscParamBuffer(
    VAEncMiscParameterType type,
    scoped_refptr<base::RefCountedBytes> buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);

  const size_t temp_size = sizeof(VAEncMiscParameterBuffer) + buffer->size();
  std::vector<uint8_t> temp(temp_size);

  auto* const va_buffer =
      reinterpret_cast<VAEncMiscParameterBuffer*>(temp.data());
  va_buffer->type = type;
  memcpy(va_buffer->data, buffer->front(), buffer->size());

  if (!vaapi_wrapper_->SubmitBuffer(VAEncMiscParameterBufferType, temp_size,
                                    temp.data())) {
    NOTIFY_ERROR(kPlatformFailureError, "Failed submitting a buffer");
  }
}

void VaapiVideoEncodeAccelerator::SubmitH264BitstreamBuffer(
    scoped_refptr<H264BitstreamBuffer> buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);

  if (!vaapi_wrapper_->SubmitBuffer(VAEncPackedHeaderDataBufferType,
                                    buffer->BytesInBuffer(), buffer->data())) {
    NOTIFY_ERROR(kPlatformFailureError, "Failed submitting a bitstream buffer");
  }
}

void VaapiVideoEncodeAccelerator::NotifyEncodedChunkSize(
    VABufferID buffer_id,
    VASurfaceID sync_surface_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_checker_);
  const uint64_t encoded_chunk_size =
      vaapi_wrapper_->GetEncodedChunkSize(buffer_id, sync_surface_id);
  if (encoded_chunk_size == 0)
    NOTIFY_ERROR(kPlatformFailureError, "Failed getting an encoded chunksize");

  DCHECK(encoder_);
  encoder_->BitrateControlUpdate(encoded_chunk_size);
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
    std::unique_ptr<VaapiEncodeJob> encode_job,
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

std::unique_ptr<VaapiEncodeJob> VaapiVideoEncodeAccelerator::CreateEncodeJob(
    scoped_refptr<VideoFrame> frame,
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

  auto job = std::make_unique<VaapiEncodeJob>(
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
    std::unique_ptr<VaapiEncodeJob> job;
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

  // Note ScopedVABuffer in VaapiEncodeJob must be destroyed before
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

VaapiEncodeJob::VaapiEncodeJob(scoped_refptr<VideoFrame> input_frame,
                               bool keyframe,
                               base::OnceClosure execute_cb,
                               scoped_refptr<VASurface> input_surface,
                               scoped_refptr<CodecPicture> picture,
                               std::unique_ptr<ScopedVABuffer> coded_buffer)
    : EncodeJob(input_frame, keyframe, std::move(execute_cb)),
      input_surface_(input_surface),
      picture_(std::move(picture)),
      coded_buffer_(std::move(coded_buffer)) {
  DCHECK(input_surface_);
  DCHECK(picture_);
  DCHECK(coded_buffer_);
}

static void InitVAPictureH264(VAPictureH264* va_pic) {
  *va_pic = {};
  va_pic->picture_id = VA_INVALID_ID;
  va_pic->flags = VA_PICTURE_H264_INVALID;
}

static scoped_refptr<base::RefCountedBytes> MakeRefCountedBytes(void* ptr,
                                                                size_t size) {
  return base::MakeRefCounted<base::RefCountedBytes>(
      reinterpret_cast<uint8_t*>(ptr), size);
}

bool VaapiVideoEncodeAccelerator::H264Accelerator::SubmitFrameParameters(
    AcceleratedVideoEncoder::EncodeJob* job,
    const H264Encoder::EncodeParams& encode_params,
    const H264SPS& sps,
    const H264PPS& pps,
    scoped_refptr<H264Picture> pic,
    const std::list<scoped_refptr<H264Picture>>& ref_pic_list0,
    const std::list<scoped_refptr<H264Picture>>& ref_pic_list1) {
  VAEncSequenceParameterBufferH264 seq_param = {};

#define SPS_TO_SP(a) seq_param.a = sps.a;
  SPS_TO_SP(seq_parameter_set_id);
  SPS_TO_SP(level_idc);

  seq_param.intra_period = encode_params.i_period_frames;
  seq_param.intra_idr_period = encode_params.idr_period_frames;
  seq_param.ip_period = encode_params.ip_period_frames;
  seq_param.bits_per_second = encode_params.bitrate_bps;

  SPS_TO_SP(max_num_ref_frames);
  base::Optional<gfx::Size> coded_size = sps.GetCodedSize();
  if (!coded_size) {
    DVLOGF(1) << "Invalid coded size";
    return false;
  }
  constexpr int kH264MacroblockSizeInPixels = 16;
  seq_param.picture_width_in_mbs =
      coded_size->width() / kH264MacroblockSizeInPixels;
  seq_param.picture_height_in_mbs =
      coded_size->height() / kH264MacroblockSizeInPixels;

#define SPS_TO_SP_FS(a) seq_param.seq_fields.bits.a = sps.a;
  SPS_TO_SP_FS(chroma_format_idc);
  SPS_TO_SP_FS(frame_mbs_only_flag);
  SPS_TO_SP_FS(log2_max_frame_num_minus4);
  SPS_TO_SP_FS(pic_order_cnt_type);
  SPS_TO_SP_FS(log2_max_pic_order_cnt_lsb_minus4);
#undef SPS_TO_SP_FS

  SPS_TO_SP(bit_depth_luma_minus8);
  SPS_TO_SP(bit_depth_chroma_minus8);

  SPS_TO_SP(frame_cropping_flag);
  if (sps.frame_cropping_flag) {
    SPS_TO_SP(frame_crop_left_offset);
    SPS_TO_SP(frame_crop_right_offset);
    SPS_TO_SP(frame_crop_top_offset);
    SPS_TO_SP(frame_crop_bottom_offset);
  }

  SPS_TO_SP(vui_parameters_present_flag);
#define SPS_TO_SP_VF(a) seq_param.vui_fields.bits.a = sps.a;
  SPS_TO_SP_VF(timing_info_present_flag);
#undef SPS_TO_SP_VF
  SPS_TO_SP(num_units_in_tick);
  SPS_TO_SP(time_scale);
#undef SPS_TO_SP

  job->AddSetupCallback(
      base::BindOnce(&VaapiVideoEncodeAccelerator::SubmitBuffer,
                     base::Unretained(vea_), VAEncSequenceParameterBufferType,
                     MakeRefCountedBytes(&seq_param, sizeof(seq_param))));

  VAEncPictureParameterBufferH264 pic_param = {};

  auto va_surface_id = pic->AsVaapiH264Picture()->GetVASurfaceID();
  pic_param.CurrPic.picture_id = va_surface_id;
  pic_param.CurrPic.TopFieldOrderCnt = pic->top_field_order_cnt;
  pic_param.CurrPic.BottomFieldOrderCnt = pic->bottom_field_order_cnt;
  pic_param.CurrPic.flags = 0;

  pic_param.coded_buf = job->AsVaapiEncodeJob()->coded_buffer_id();
  pic_param.pic_parameter_set_id = pps.pic_parameter_set_id;
  pic_param.seq_parameter_set_id = pps.seq_parameter_set_id;
  pic_param.frame_num = pic->frame_num;
  pic_param.pic_init_qp = pps.pic_init_qp_minus26 + 26;
  pic_param.num_ref_idx_l0_active_minus1 =
      pps.num_ref_idx_l0_default_active_minus1;

  pic_param.pic_fields.bits.idr_pic_flag = pic->idr;
  pic_param.pic_fields.bits.reference_pic_flag = pic->ref;
#define PPS_TO_PP_PF(a) pic_param.pic_fields.bits.a = pps.a;
  PPS_TO_PP_PF(entropy_coding_mode_flag);
  PPS_TO_PP_PF(transform_8x8_mode_flag);
  PPS_TO_PP_PF(deblocking_filter_control_present_flag);
#undef PPS_TO_PP_PF

  VAEncSliceParameterBufferH264 slice_param = {};

  slice_param.num_macroblocks =
      seq_param.picture_width_in_mbs * seq_param.picture_height_in_mbs;
  slice_param.macroblock_info = VA_INVALID_ID;
  slice_param.slice_type = pic->type;
  slice_param.pic_parameter_set_id = pps.pic_parameter_set_id;
  slice_param.idr_pic_id = pic->idr_pic_id;
  slice_param.pic_order_cnt_lsb = pic->pic_order_cnt_lsb;
  slice_param.num_ref_idx_active_override_flag = true;

  for (size_t i = 0; i < base::size(pic_param.ReferenceFrames); ++i)
    InitVAPictureH264(&pic_param.ReferenceFrames[i]);

  for (size_t i = 0; i < base::size(slice_param.RefPicList0); ++i)
    InitVAPictureH264(&slice_param.RefPicList0[i]);

  for (size_t i = 0; i < base::size(slice_param.RefPicList1); ++i)
    InitVAPictureH264(&slice_param.RefPicList1[i]);

  VAPictureH264* ref_frames_entry = pic_param.ReferenceFrames;
  VAPictureH264* ref_list_entry = slice_param.RefPicList0;
  // Initialize the current entry on slice and picture reference lists to
  // |ref_pic| and advance list pointers.
  auto fill_ref_frame = [&ref_frames_entry,
                         &ref_list_entry](scoped_refptr<H264Picture> ref_pic) {
    VAPictureH264 va_pic_h264;
    InitVAPictureH264(&va_pic_h264);
    va_pic_h264.picture_id = ref_pic->AsVaapiH264Picture()->GetVASurfaceID();
    va_pic_h264.flags = 0;

    *ref_frames_entry = va_pic_h264;
    *ref_list_entry = va_pic_h264;
    ++ref_frames_entry;
    ++ref_list_entry;
  };

  // Fill slice_param.RefPicList{0,1} with pictures from ref_pic_list{0,1},
  // respectively, and pic_param.ReferenceFrames with entries from both.
  std::for_each(ref_pic_list0.begin(), ref_pic_list0.end(), fill_ref_frame);
  ref_list_entry = slice_param.RefPicList1;
  std::for_each(ref_pic_list1.begin(), ref_pic_list1.end(), fill_ref_frame);

  VAEncMiscParameterRateControl rate_control_param = {};
  rate_control_param.bits_per_second = encode_params.bitrate_bps;
  rate_control_param.target_percentage = kTargetBitratePercentage;
  rate_control_param.window_size = encode_params.cpb_window_size_ms;
  rate_control_param.initial_qp = pic_param.pic_init_qp;
  rate_control_param.min_qp = encode_params.scaling_settings.min_qp;
  rate_control_param.max_qp = encode_params.scaling_settings.max_qp;
  rate_control_param.rc_flags.bits.disable_frame_skip = true;

  VAEncMiscParameterFrameRate framerate_param = {};
  framerate_param.framerate = encode_params.framerate;

  VAEncMiscParameterHRD hrd_param = {};
  hrd_param.buffer_size = encode_params.cpb_size_bits;
  hrd_param.initial_buffer_fullness = hrd_param.buffer_size / 2;

  job->AddSetupCallback(
      base::BindOnce(&VaapiVideoEncodeAccelerator::SubmitBuffer,
                     base::Unretained(vea_), VAEncPictureParameterBufferType,
                     MakeRefCountedBytes(&pic_param, sizeof(pic_param))));

  job->AddSetupCallback(
      base::BindOnce(&VaapiVideoEncodeAccelerator::SubmitBuffer,
                     base::Unretained(vea_), VAEncSliceParameterBufferType,
                     MakeRefCountedBytes(&slice_param, sizeof(slice_param))));

  job->AddSetupCallback(base::BindOnce(
      &VaapiVideoEncodeAccelerator::SubmitVAEncMiscParamBuffer,
      base::Unretained(vea_), VAEncMiscParameterTypeRateControl,
      MakeRefCountedBytes(&rate_control_param, sizeof(rate_control_param))));

  job->AddSetupCallback(base::BindOnce(
      &VaapiVideoEncodeAccelerator::SubmitVAEncMiscParamBuffer,
      base::Unretained(vea_), VAEncMiscParameterTypeFrameRate,
      MakeRefCountedBytes(&framerate_param, sizeof(framerate_param))));

  job->AddSetupCallback(
      base::BindOnce(&VaapiVideoEncodeAccelerator::SubmitVAEncMiscParamBuffer,
                     base::Unretained(vea_), VAEncMiscParameterTypeHRD,
                     MakeRefCountedBytes(&hrd_param, sizeof(hrd_param))));

  return true;
}

scoped_refptr<H264Picture>
VaapiVideoEncodeAccelerator::H264Accelerator::GetPicture(
    AcceleratedVideoEncoder::EncodeJob* job) {
  return base::WrapRefCounted(
      reinterpret_cast<H264Picture*>(job->AsVaapiEncodeJob()->picture().get()));
}

bool VaapiVideoEncodeAccelerator::H264Accelerator::SubmitPackedHeaders(
    AcceleratedVideoEncoder::EncodeJob* job,
    scoped_refptr<H264BitstreamBuffer> packed_sps,
    scoped_refptr<H264BitstreamBuffer> packed_pps) {
  // Submit SPS.
  VAEncPackedHeaderParameterBuffer par_buffer = {};
  par_buffer.type = VAEncPackedHeaderSequence;
  par_buffer.bit_length = packed_sps->BytesInBuffer() * 8;

  job->AddSetupCallback(base::BindOnce(
      &VaapiVideoEncodeAccelerator::SubmitBuffer, base::Unretained(vea_),
      VAEncPackedHeaderParameterBufferType,
      MakeRefCountedBytes(&par_buffer, sizeof(par_buffer))));

  job->AddSetupCallback(
      base::BindOnce(&VaapiVideoEncodeAccelerator::SubmitH264BitstreamBuffer,
                     base::Unretained(vea_), packed_sps));

  // Submit PPS.
  par_buffer = {};
  par_buffer.type = VAEncPackedHeaderPicture;
  par_buffer.bit_length = packed_pps->BytesInBuffer() * 8;

  job->AddSetupCallback(base::BindOnce(
      &VaapiVideoEncodeAccelerator::SubmitBuffer, base::Unretained(vea_),
      VAEncPackedHeaderParameterBufferType,
      MakeRefCountedBytes(&par_buffer, sizeof(par_buffer))));

  job->AddSetupCallback(
      base::BindOnce(&VaapiVideoEncodeAccelerator::SubmitH264BitstreamBuffer,
                     base::Unretained(vea_), packed_pps));

  return true;
}

scoped_refptr<VP8Picture>
VaapiVideoEncodeAccelerator::VP8Accelerator::GetPicture(
    AcceleratedVideoEncoder::EncodeJob* job) {
  return base::WrapRefCounted(
      reinterpret_cast<VP8Picture*>(job->AsVaapiEncodeJob()->picture().get()));
}

bool VaapiVideoEncodeAccelerator::VP8Accelerator::SubmitFrameParameters(
    AcceleratedVideoEncoder::EncodeJob* job,
    const VP8Encoder::EncodeParams& encode_params,
    scoped_refptr<VP8Picture> pic,
    const Vp8ReferenceFrameVector& ref_frames,
    const std::array<bool, kNumVp8ReferenceBuffers>& ref_frames_used) {
  VAEncSequenceParameterBufferVP8 seq_param = {};

  const auto& frame_header = pic->frame_hdr;
  seq_param.frame_width = frame_header->width;
  seq_param.frame_height = frame_header->height;
  seq_param.frame_width_scale = frame_header->horizontal_scale;
  seq_param.frame_height_scale = frame_header->vertical_scale;
  seq_param.error_resilient = 1;
  seq_param.bits_per_second = encode_params.bitrate_allocation.GetSumBps();
  seq_param.intra_period = encode_params.kf_period_frames;

  VAEncPictureParameterBufferVP8 pic_param = {};

  pic_param.reconstructed_frame = pic->AsVaapiVP8Picture()->GetVASurfaceID();
  DCHECK_NE(pic_param.reconstructed_frame, VA_INVALID_ID);

  auto last_frame = ref_frames.GetFrame(Vp8RefType::VP8_FRAME_LAST);
  pic_param.ref_last_frame =
      last_frame ? last_frame->AsVaapiVP8Picture()->GetVASurfaceID()
                 : VA_INVALID_ID;
  auto golden_frame = ref_frames.GetFrame(Vp8RefType::VP8_FRAME_GOLDEN);
  pic_param.ref_gf_frame =
      golden_frame ? golden_frame->AsVaapiVP8Picture()->GetVASurfaceID()
                   : VA_INVALID_ID;
  auto alt_frame = ref_frames.GetFrame(Vp8RefType::VP8_FRAME_ALTREF);
  pic_param.ref_arf_frame =
      alt_frame ? alt_frame->AsVaapiVP8Picture()->GetVASurfaceID()
                : VA_INVALID_ID;
  pic_param.coded_buf = job->AsVaapiEncodeJob()->coded_buffer_id();
  DCHECK_NE(pic_param.coded_buf, VA_INVALID_ID);
  pic_param.ref_flags.bits.no_ref_last =
      !ref_frames_used[Vp8RefType::VP8_FRAME_LAST];
  pic_param.ref_flags.bits.no_ref_gf =
      !ref_frames_used[Vp8RefType::VP8_FRAME_GOLDEN];
  pic_param.ref_flags.bits.no_ref_arf =
      !ref_frames_used[Vp8RefType::VP8_FRAME_ALTREF];

  if (frame_header->IsKeyframe()) {
    pic_param.ref_flags.bits.force_kf = true;
  }

  pic_param.pic_flags.bits.frame_type = frame_header->frame_type;
  pic_param.pic_flags.bits.version = frame_header->version;
  pic_param.pic_flags.bits.show_frame = frame_header->show_frame;
  pic_param.pic_flags.bits.loop_filter_type = frame_header->loopfilter_hdr.type;
  pic_param.pic_flags.bits.num_token_partitions =
      frame_header->num_of_dct_partitions;
  pic_param.pic_flags.bits.segmentation_enabled =
      frame_header->segmentation_hdr.segmentation_enabled;
  pic_param.pic_flags.bits.update_mb_segmentation_map =
      frame_header->segmentation_hdr.update_mb_segmentation_map;
  pic_param.pic_flags.bits.update_segment_feature_data =
      frame_header->segmentation_hdr.update_segment_feature_data;

  pic_param.pic_flags.bits.loop_filter_adj_enable =
      frame_header->loopfilter_hdr.loop_filter_adj_enable;

  pic_param.pic_flags.bits.refresh_entropy_probs =
      frame_header->refresh_entropy_probs;
  pic_param.pic_flags.bits.refresh_golden_frame =
      frame_header->refresh_golden_frame;
  pic_param.pic_flags.bits.refresh_alternate_frame =
      frame_header->refresh_alternate_frame;
  pic_param.pic_flags.bits.refresh_last = frame_header->refresh_last;
  pic_param.pic_flags.bits.copy_buffer_to_golden =
      frame_header->copy_buffer_to_golden;
  pic_param.pic_flags.bits.copy_buffer_to_alternate =
      frame_header->copy_buffer_to_alternate;
  pic_param.pic_flags.bits.sign_bias_golden = frame_header->sign_bias_golden;
  pic_param.pic_flags.bits.sign_bias_alternate =
      frame_header->sign_bias_alternate;
  pic_param.pic_flags.bits.mb_no_coeff_skip = frame_header->mb_no_skip_coeff;
  if (frame_header->IsKeyframe())
    pic_param.pic_flags.bits.forced_lf_adjustment = true;

  static_assert(std::extent<decltype(pic_param.loop_filter_level)>() ==
                        std::extent<decltype(pic_param.ref_lf_delta)>() &&
                    std::extent<decltype(pic_param.ref_lf_delta)>() ==
                        std::extent<decltype(pic_param.mode_lf_delta)>() &&
                    std::extent<decltype(pic_param.ref_lf_delta)>() ==
                        std::extent<decltype(
                            frame_header->loopfilter_hdr.ref_frame_delta)>() &&
                    std::extent<decltype(pic_param.mode_lf_delta)>() ==
                        std::extent<decltype(
                            frame_header->loopfilter_hdr.mb_mode_delta)>(),
                "Invalid loop filter array sizes");

  for (size_t i = 0; i < base::size(pic_param.loop_filter_level); ++i) {
    pic_param.loop_filter_level[i] = frame_header->loopfilter_hdr.level;
    pic_param.ref_lf_delta[i] = frame_header->loopfilter_hdr.ref_frame_delta[i];
    pic_param.mode_lf_delta[i] = frame_header->loopfilter_hdr.mb_mode_delta[i];
  }

  pic_param.sharpness_level = frame_header->loopfilter_hdr.sharpness_level;
  pic_param.clamp_qindex_high = encode_params.scaling_settings.max_qp;
  pic_param.clamp_qindex_low = encode_params.scaling_settings.min_qp;

  VAQMatrixBufferVP8 qmatrix_buf = {};
  for (size_t i = 0; i < base::size(qmatrix_buf.quantization_index); ++i)
    qmatrix_buf.quantization_index[i] = frame_header->quantization_hdr.y_ac_qi;

  qmatrix_buf.quantization_index_delta[0] =
      frame_header->quantization_hdr.y_dc_delta;
  qmatrix_buf.quantization_index_delta[1] =
      frame_header->quantization_hdr.y2_dc_delta;
  qmatrix_buf.quantization_index_delta[2] =
      frame_header->quantization_hdr.y2_ac_delta;
  qmatrix_buf.quantization_index_delta[3] =
      frame_header->quantization_hdr.uv_dc_delta;
  qmatrix_buf.quantization_index_delta[4] =
      frame_header->quantization_hdr.uv_ac_delta;

  VAEncMiscParameterRateControl rate_control_param = {};
  rate_control_param.bits_per_second =
      encode_params.bitrate_allocation.GetSumBps();
  rate_control_param.target_percentage = kTargetBitratePercentage;
  rate_control_param.window_size = encode_params.cpb_window_size_ms;
  rate_control_param.initial_qp = encode_params.initial_qp;
  rate_control_param.min_qp = encode_params.scaling_settings.min_qp;
  rate_control_param.max_qp = encode_params.scaling_settings.max_qp;
  rate_control_param.rc_flags.bits.disable_frame_skip = true;

  VAEncMiscParameterFrameRate framerate_param = {};
  framerate_param.framerate = encode_params.framerate;

  VAEncMiscParameterHRD hrd_param = {};
  hrd_param.buffer_size = encode_params.cpb_size_bits;
  hrd_param.initial_buffer_fullness = hrd_param.buffer_size / 2;

  job->AddSetupCallback(
      base::BindOnce(&VaapiVideoEncodeAccelerator::SubmitBuffer,
                     base::Unretained(vea_), VAEncSequenceParameterBufferType,
                     MakeRefCountedBytes(&seq_param, sizeof(seq_param))));

  job->AddSetupCallback(
      base::BindOnce(&VaapiVideoEncodeAccelerator::SubmitBuffer,
                     base::Unretained(vea_), VAEncPictureParameterBufferType,
                     MakeRefCountedBytes(&pic_param, sizeof(pic_param))));

  job->AddSetupCallback(
      base::BindOnce(&VaapiVideoEncodeAccelerator::SubmitBuffer,
                     base::Unretained(vea_), VAQMatrixBufferType,
                     MakeRefCountedBytes(&qmatrix_buf, sizeof(qmatrix_buf))));

  job->AddSetupCallback(base::BindOnce(
      &VaapiVideoEncodeAccelerator::SubmitVAEncMiscParamBuffer,
      base::Unretained(vea_), VAEncMiscParameterTypeRateControl,
      MakeRefCountedBytes(&rate_control_param, sizeof(rate_control_param))));

  job->AddSetupCallback(base::BindOnce(
      &VaapiVideoEncodeAccelerator::SubmitVAEncMiscParamBuffer,
      base::Unretained(vea_), VAEncMiscParameterTypeFrameRate,
      MakeRefCountedBytes(&framerate_param, sizeof(framerate_param))));

  job->AddSetupCallback(
      base::BindOnce(&VaapiVideoEncodeAccelerator::SubmitVAEncMiscParamBuffer,
                     base::Unretained(vea_), VAEncMiscParameterTypeHRD,
                     MakeRefCountedBytes(&hrd_param, sizeof(hrd_param))));

  return true;
}

scoped_refptr<VP9Picture>
VaapiVideoEncodeAccelerator::VP9Accelerator::GetPicture(
    AcceleratedVideoEncoder::EncodeJob* job) {
  return base::WrapRefCounted(
      reinterpret_cast<VP9Picture*>(job->AsVaapiEncodeJob()->picture().get()));
}

bool VaapiVideoEncodeAccelerator::VP9Accelerator::SubmitFrameParameters(
    AcceleratedVideoEncoder::EncodeJob* job,
    const VP9Encoder::EncodeParams& encode_params,
    scoped_refptr<VP9Picture> pic,
    const Vp9ReferenceFrameVector& ref_frames,
    const std::array<bool, kVp9NumRefsPerFrame>& ref_frames_used) {
  VAEncSequenceParameterBufferVP9 seq_param = {};

  const auto& frame_header = pic->frame_hdr;
  // TODO(crbug.com/811912): Double check whether the
  // max_frame_width or max_frame_height affects any of the memory
  // allocation and tighten these values based on that.
  constexpr gfx::Size kMaxFrameSize(4096, 4096);
  seq_param.max_frame_width = kMaxFrameSize.height();
  seq_param.max_frame_height = kMaxFrameSize.width();
  seq_param.bits_per_second = encode_params.bitrate_allocation.GetSumBps();
  seq_param.intra_period = encode_params.kf_period_frames;

  VAEncPictureParameterBufferVP9 pic_param = {};

  pic_param.frame_width_src = frame_header->frame_width;
  pic_param.frame_height_src = frame_header->frame_height;
  pic_param.frame_width_dst = frame_header->render_width;
  pic_param.frame_height_dst = frame_header->render_height;

  pic_param.reconstructed_frame = pic->AsVaapiVP9Picture()->GetVASurfaceID();
  DCHECK_NE(pic_param.reconstructed_frame, VA_INVALID_ID);

  for (size_t i = 0; i < kVp9NumRefFrames; i++) {
    auto ref_pic = ref_frames.GetFrame(i);
    pic_param.reference_frames[i] =
        ref_pic ? ref_pic->AsVaapiVP9Picture()->GetVASurfaceID()
                : VA_INVALID_ID;
  }

  pic_param.coded_buf = job->AsVaapiEncodeJob()->coded_buffer_id();
  DCHECK_NE(pic_param.coded_buf, VA_INVALID_ID);

  if (frame_header->IsKeyframe()) {
    pic_param.ref_flags.bits.force_kf = true;
  } else {
    for (size_t i = 0; i < kVp9NumRefsPerFrame; i++) {
      if (ref_frames_used[i])
        pic_param.ref_flags.bits.ref_frame_ctrl_l0 |= (1 << i);
    }

    if (ref_frames_used[0])
      pic_param.ref_flags.bits.ref_last_idx = frame_header->ref_frame_idx[0];
    if (ref_frames_used[1])
      pic_param.ref_flags.bits.ref_gf_idx = frame_header->ref_frame_idx[1];
    if (ref_frames_used[2])
      pic_param.ref_flags.bits.ref_arf_idx = frame_header->ref_frame_idx[2];
  }

  pic_param.pic_flags.bits.frame_type = frame_header->frame_type;
  pic_param.pic_flags.bits.show_frame = frame_header->show_frame;
  pic_param.pic_flags.bits.error_resilient_mode =
      frame_header->error_resilient_mode;
  pic_param.pic_flags.bits.intra_only = frame_header->intra_only;
  pic_param.pic_flags.bits.allow_high_precision_mv =
      frame_header->allow_high_precision_mv;
  pic_param.pic_flags.bits.mcomp_filter_type =
      frame_header->interpolation_filter;
  pic_param.pic_flags.bits.frame_parallel_decoding_mode =
      frame_header->frame_parallel_decoding_mode;
  pic_param.pic_flags.bits.reset_frame_context =
      frame_header->reset_frame_context;
  pic_param.pic_flags.bits.refresh_frame_context =
      frame_header->refresh_frame_context;
  pic_param.pic_flags.bits.frame_context_idx = frame_header->frame_context_idx;

  pic_param.refresh_frame_flags = frame_header->refresh_frame_flags;

  pic_param.luma_ac_qindex = frame_header->quant_params.base_q_idx;
  pic_param.luma_dc_qindex_delta = frame_header->quant_params.delta_q_y_dc;
  pic_param.chroma_ac_qindex_delta = frame_header->quant_params.delta_q_uv_ac;
  pic_param.chroma_dc_qindex_delta = frame_header->quant_params.delta_q_uv_dc;
  pic_param.filter_level = frame_header->loop_filter.level;
  pic_param.log2_tile_rows = frame_header->tile_rows_log2;
  pic_param.log2_tile_columns = frame_header->tile_cols_log2;

  job->AddSetupCallback(
      base::BindOnce(&VaapiVideoEncodeAccelerator::SubmitBuffer,
                     base::Unretained(vea_), VAEncSequenceParameterBufferType,
                     MakeRefCountedBytes(&seq_param, sizeof(seq_param))));

  job->AddSetupCallback(
      base::BindOnce(&VaapiVideoEncodeAccelerator::SubmitBuffer,
                     base::Unretained(vea_), VAEncPictureParameterBufferType,
                     MakeRefCountedBytes(&pic_param, sizeof(pic_param))));

  if (bitrate_control_ ==
      AcceleratedVideoEncoder::BitrateControl::kConstantQuantizationParameter) {
    job->AddPostExecuteCallback(base::BindOnce(
        &VaapiVideoEncodeAccelerator::NotifyEncodedChunkSize,
        base::Unretained(vea_), job->AsVaapiEncodeJob()->coded_buffer_id(),
        job->AsVaapiEncodeJob()->input_surface()->id()));
    return true;
  }

  VAEncMiscParameterRateControl rate_control_param = {};
  rate_control_param.bits_per_second =
      encode_params.bitrate_allocation.GetSumBps();
  rate_control_param.target_percentage = kTargetBitratePercentage;
  rate_control_param.window_size = encode_params.cpb_window_size_ms;
  rate_control_param.initial_qp = encode_params.initial_qp;
  rate_control_param.min_qp = encode_params.scaling_settings.min_qp;
  rate_control_param.max_qp = encode_params.scaling_settings.max_qp;
  rate_control_param.rc_flags.bits.disable_frame_skip = true;

  VAEncMiscParameterFrameRate framerate_param = {};
  framerate_param.framerate = encode_params.framerate;

  VAEncMiscParameterHRD hrd_param = {};
  hrd_param.buffer_size = encode_params.cpb_size_bits;
  hrd_param.initial_buffer_fullness = hrd_param.buffer_size / 2;

  job->AddSetupCallback(base::BindOnce(
      &VaapiVideoEncodeAccelerator::SubmitVAEncMiscParamBuffer,
      base::Unretained(vea_), VAEncMiscParameterTypeRateControl,
      MakeRefCountedBytes(&rate_control_param, sizeof(rate_control_param))));

  job->AddSetupCallback(base::BindOnce(
      &VaapiVideoEncodeAccelerator::SubmitVAEncMiscParamBuffer,
      base::Unretained(vea_), VAEncMiscParameterTypeFrameRate,
      MakeRefCountedBytes(&framerate_param, sizeof(framerate_param))));

  job->AddSetupCallback(
      base::BindOnce(&VaapiVideoEncodeAccelerator::SubmitVAEncMiscParamBuffer,
                     base::Unretained(vea_), VAEncMiscParameterTypeHRD,
                     MakeRefCountedBytes(&hrd_param, sizeof(hrd_param))));

  return true;
}

}  // namespace media

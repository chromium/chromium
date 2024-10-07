// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/video_encode_accelerator_adapter.h"

#include <limits>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/svc_scalability_mode.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
#include "media/formats/mp4/h264_annex_b_to_avc_bitstream_converter.h"
#if BUILDFLAG(ENABLE_PLATFORM_HEVC) && \
    BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
#include "media/formats/mp4/h265_annex_b_to_hevc_bitstream_converter.h"
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC) &&
        // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
#include "media/video/gpu_video_accelerator_factories.h"
#include "media/video/video_encoder_info.h"

namespace media {

namespace {

// HW encoders expect a nonzero bitrate, so |kVEADefaultBitratePerPixel| is used
// to estimate bits per second for ~30 fps with ~1/16 compression rate.
constexpr int kVEADefaultBitratePerPixel = 2;

uint32_t ComputeCheckedDefaultBitrate(const gfx::Size& frame_size) {
  base::CheckedNumeric<uint32_t> checked_bitrate_product =
      base::CheckMul<uint32_t>(frame_size.width(), frame_size.height(),
                               kVEADefaultBitratePerPixel);
  // If the product has overflowed, clamp it to uint32_t max
  return checked_bitrate_product.ValueOrDefault(
      std::numeric_limits<uint32_t>::max());
}

uint32_t ComputeCheckedPeakBitrate(uint32_t target_bitrate) {
  // TODO(crbug.com/40851972): Reconsider whether this is good peak bps.
  base::CheckedNumeric<uint32_t> checked_bitrate_product =
      base::CheckMul<uint32_t>(target_bitrate, 10u);
  return checked_bitrate_product.ValueOrDefault(
      std::numeric_limits<uint32_t>::max());
}

Bitrate CreateBitrate(
    const std::optional<Bitrate>& requested_bitrate,
    const gfx::Size& frame_size,
    VideoEncodeAccelerator::SupportedRateControlMode supported_rc_modes) {
  uint32_t default_bitrate = ComputeCheckedDefaultBitrate(frame_size);
  if (supported_rc_modes & VideoEncodeAccelerator::kVariableMode) {
    // VEA supports VBR. Use |requested_bitrate| or VBR if bitrate is not
    // specified.
    return requested_bitrate.value_or(Bitrate::VariableBitrate(
        default_bitrate, ComputeCheckedPeakBitrate(default_bitrate)));
  }
  // VEA doesn't support VBR. The bitrate configured to VEA must be CBR. In
  // other words, if |requested_bitrate| is CBR, bitrate mode fallbacks to VBR.
  if (requested_bitrate &&
      requested_bitrate->mode() == Bitrate::Mode::kConstant) {
    return *requested_bitrate;
  }

  return Bitrate::ConstantBitrate(
      requested_bitrate ? requested_bitrate->target_bps() : default_bitrate);
}

VideoEncodeAccelerator::Config SetUpVeaConfig(
    VideoCodecProfile profile,
    const VideoEncoder::Options& opts,
    VideoPixelFormat format,
    VideoFrame::StorageType storage_type,
    VideoEncodeAccelerator::SupportedRateControlMode supported_rc_modes,
    VideoEncodeAccelerator::Config::EncoderType required_encoder_type,
    bool is_gpu_supported_format) {
  Bitrate bitrate =
      CreateBitrate(opts.bitrate, opts.frame_size, supported_rc_modes);
  auto config = VideoEncodeAccelerator::Config(
      format, opts.frame_size, profile, bitrate,
      opts.framerate.value_or(VideoEncodeAccelerator::kDefaultFramerate),
      VideoEncodeAccelerator::Config::StorageType::kShmem,
      VideoEncodeAccelerator::Config::ContentType::kCamera);
  config.gop_length = opts.keyframe_interval;

  if (opts.content_hint) {
    switch (*opts.content_hint) {
      case media::VideoEncoder::ContentHint::Camera:
        config.content_type =
            VideoEncodeAccelerator::Config::ContentType::kCamera;
        break;
      case media::VideoEncoder::ContentHint::Screen:
        config.content_type =
            VideoEncodeAccelerator::Config::ContentType::kDisplay;
        break;
    }
  }

  if (opts.latency_mode == VideoEncoder::LatencyMode::Realtime) {
    config.drop_frame_thresh_percentage =
        GetDefaultVideoEncoderDropFrameThreshold();
  }

  size_t num_temporal_layers = 1;
  if (opts.scalability_mode) {
    switch (opts.scalability_mode.value()) {
      case SVCScalabilityMode::kL1T1:
        // Nothing to do
        break;
      case SVCScalabilityMode::kL1T2:
        num_temporal_layers = 2;
        break;
      case SVCScalabilityMode::kL1T3:
        num_temporal_layers = 3;
        break;
      default:
        NOTREACHED_IN_MIGRATION()
            << "Unsupported SVC: "
            << GetScalabilityModeName(opts.scalability_mode.value());
    }
  }
  if (num_temporal_layers > 1) {
    VideoEncodeAccelerator::Config::SpatialLayer layer;
    layer.width = opts.frame_size.width();
    layer.height = opts.frame_size.height();
    layer.bitrate_bps = config.bitrate.target_bps();
    layer.framerate = config.framerate;
    layer.num_of_temporal_layers = num_temporal_layers;
    config.spatial_layers.push_back(layer);
  }

  config.require_low_delay =
      opts.latency_mode == VideoEncoder::LatencyMode::Realtime;
  config.required_encoder_type = required_encoder_type;

  const bool is_rgb =
      format == PIXEL_FORMAT_XBGR || format == PIXEL_FORMAT_XRGB ||
      format == PIXEL_FORMAT_ABGR || format == PIXEL_FORMAT_ARGB;

  // Override the provided format if incoming frames are RGB -- they'll be
  // converted to I420 or NV12 depending on the VEA configuration.
  if (is_rgb && !is_gpu_supported_format) {
    config.input_format = PIXEL_FORMAT_I420;
  }

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  if (format != PIXEL_FORMAT_I420 ||
      !VideoFrame::IsStorageTypeMappable(storage_type)) {
    // ChromeOS/Linux hardware video encoders supports I420 on-memory
    // VideoFrame and NV12 GpuMemoryBuffer VideoFrame.
    // For other VideoFrames than them, some processing e.g. format conversion
    // is required. Let the destination buffer be GpuMemoryBuffer because a
    // hardware encoder can process it more efficiently than on-memory buffer.
    config.input_format = PIXEL_FORMAT_NV12;
    config.storage_type =
        VideoEncodeAccelerator::Config::StorageType::kGpuMemoryBuffer;
  }
#endif

  return config;
}

}  // namespace

class VideoEncodeAcceleratorAdapter::GpuMemoryBufferVideoFramePool
    : public base::RefCountedThreadSafe<GpuMemoryBufferVideoFramePool> {
 public:
  GpuMemoryBufferVideoFramePool(GpuVideoAcceleratorFactories* gpu_factories,
                                const gfx::Size& coded_size)
      : gpu_factories_(gpu_factories), coded_size_(coded_size) {}
  GpuMemoryBufferVideoFramePool(const GpuMemoryBufferVideoFramePool&) = delete;
  GpuMemoryBufferVideoFramePool& operator=(
      const GpuMemoryBufferVideoFramePool&) = delete;

  scoped_refptr<VideoFrame> MaybeCreateVideoFrame(
      const gfx::Size& visible_size) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(gfx::Rect(coded_size_).Contains(gfx::Rect(visible_size)));

    const auto buffer_format = gfx::BufferFormat::YUV_420_BIPLANAR;
    const auto si_format = viz::GetSharedImageFormat(buffer_format);
    const auto buffer_usage =
        gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE;

    // Setting some default usage in order to get a mappable shared image.
    const auto si_usage = gpu::SHARED_IMAGE_USAGE_CPU_WRITE |
                          gpu::SHARED_IMAGE_USAGE_DISPLAY_READ;

    scoped_refptr<VideoFrame> video_frame;
    gpu::SyncToken sync_token;
    if (available_shared_images_.empty()) {
      auto* sii = gpu_factories_->SharedImageInterface();
      if (!sii) {
        LOG(ERROR) << "SharedImageInterface is null.";
        return nullptr;
      }

      auto shared_image = sii->CreateSharedImage(
          {si_format, coded_size_, gfx::ColorSpace(),
           gpu::SharedImageUsageSet(si_usage), "VideoEncodeAcceleratorAdapter"},
          gpu::kNullSurfaceHandle, buffer_usage);
      if (!shared_image) {
        LOG(ERROR) << "Unable to create a mappable shared image.";
        return nullptr;
      }
      sync_token = sii->GenVerifiedSyncToken();
      available_shared_images_.push_back(std::move(shared_image));
    }

    auto shared_image = std::move(available_shared_images_.back());
    available_shared_images_.pop_back();

    auto shared_image_release_cb =
        base::BindPostTaskToCurrentDefault(base::BindOnce(
            &GpuMemoryBufferVideoFramePool::ReuseFrame, this, shared_image));
    video_frame = media::VideoFrame::WrapMappableSharedImage(
        std::move(shared_image), sync_token, std::move(shared_image_release_cb),
        gfx::Rect(visible_size), visible_size, base::TimeDelta());
    return video_frame;
  }

 private:
  friend class RefCountedThreadSafe<GpuMemoryBufferVideoFramePool>;
  ~GpuMemoryBufferVideoFramePool() = default;

  // |shared_image| will be used when MappableSI is enabled. It will be null
  // otherwise.
  void ReuseFrame(scoped_refptr<gpu::ClientSharedImage> shared_image,
                  const gpu::SyncToken& token,
                  std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    constexpr size_t kMaxPooledFrames = 5;
    if (shared_image && (available_shared_images_.size() < kMaxPooledFrames)) {
      available_shared_images_.push_back(std::move(shared_image));
    }
  }

  const raw_ptr<GpuVideoAcceleratorFactories> gpu_factories_;
  const gfx::Size coded_size_;

  // Available mappable shared images.
  std::vector<scoped_refptr<gpu::ClientSharedImage>> available_shared_images_;

  SEQUENCE_CHECKER(sequence_checker_);
};

class VideoEncodeAcceleratorAdapter::ReadOnlyRegionPool
    : public base::RefCountedThreadSafe<ReadOnlyRegionPool> {
 public:
  struct Handle {
    using ReuseBufferCallback =
        base::OnceCallback<void(std::unique_ptr<base::MappedReadOnlyRegion>)>;
    Handle(std::unique_ptr<base::MappedReadOnlyRegion> mapped_region,
           ReuseBufferCallback reuse_buffer_cb)
        : owned_mapped_region(std::move(mapped_region)),
          reuse_buffer_cb(std::move(reuse_buffer_cb)) {
      DCHECK(owned_mapped_region);
    }

    ~Handle() {
      if (reuse_buffer_cb) {
        DCHECK(owned_mapped_region);
        std::move(reuse_buffer_cb).Run(std::move(owned_mapped_region));
      }
    }

    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;

    bool IsValid() const {
      return owned_mapped_region && owned_mapped_region->IsValid();
    }
    const base::ReadOnlySharedMemoryRegion* region() const {
      DCHECK(IsValid());
      return &owned_mapped_region->region;
    }
    const base::WritableSharedMemoryMapping* mapping() const {
      DCHECK(IsValid());
      return &owned_mapped_region->mapping;
    }

   private:
    std::unique_ptr<base::MappedReadOnlyRegion> owned_mapped_region;
    ReuseBufferCallback reuse_buffer_cb;
  };

  explicit ReadOnlyRegionPool(size_t buffer_size) : buffer_size_(buffer_size) {}
  ReadOnlyRegionPool(const ReadOnlyRegionPool&) = delete;
  ReadOnlyRegionPool& operator=(const ReadOnlyRegionPool&) = delete;

  std::unique_ptr<Handle> MaybeAllocateBuffer() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (available_buffers_.empty()) {
      available_buffers_.push_back(std::make_unique<base::MappedReadOnlyRegion>(
          base::ReadOnlySharedMemoryRegion::Create(buffer_size_)));
      if (!available_buffers_.back()->IsValid()) {
        available_buffers_.pop_back();
        return nullptr;
      }
    }

    auto mapped_region = std::move(available_buffers_.back());
    available_buffers_.pop_back();
    DCHECK(mapped_region->IsValid());

    return std::make_unique<Handle>(
        std::move(mapped_region),
        base::BindPostTaskToCurrentDefault(
            base::BindOnce(&ReadOnlyRegionPool::ReuseBuffer, this)));
  }

 private:
  friend class RefCountedThreadSafe<ReadOnlyRegionPool>;
  ~ReadOnlyRegionPool() = default;

  void ReuseBuffer(std::unique_ptr<base::MappedReadOnlyRegion> region) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    constexpr size_t kMaxPooledBuffers = 5;
    if (available_buffers_.size() < kMaxPooledBuffers)
      available_buffers_.push_back(std::move(region));
  }

  const size_t buffer_size_;
  std::vector<std::unique_ptr<base::MappedReadOnlyRegion>> available_buffers_;

  SEQUENCE_CHECKER(sequence_checker_);
};

VideoEncodeAcceleratorAdapter::PendingOp::PendingOp() = default;
VideoEncodeAcceleratorAdapter::PendingOp::~PendingOp() = default;

VideoEncodeAcceleratorAdapter::VideoEncodeAcceleratorAdapter(
    GpuVideoAcceleratorFactories* gpu_factories,
    std::unique_ptr<MediaLog> media_log,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    VideoEncodeAccelerator::Config::EncoderType required_encoder_type)
    : output_pool_(base::MakeRefCounted<base::UnsafeSharedMemoryPool>()),
      gpu_factories_(gpu_factories),
      media_log_(std::move(media_log)),
      accelerator_task_runner_(gpu_factories_->GetTaskRunner()),
      callback_task_runner_(std::move(callback_task_runner)),
      required_encoder_type_(required_encoder_type) {
  DETACH_FROM_SEQUENCE(accelerator_sequence_checker_);
}

VideoEncodeAcceleratorAdapter::~VideoEncodeAcceleratorAdapter() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(accelerator_sequence_checker_);
  output_pool_->Shutdown();
}

void VideoEncodeAcceleratorAdapter::DestroyAsync(
    std::unique_ptr<VideoEncodeAcceleratorAdapter> self) {
  DCHECK(self);
  auto runner = self->accelerator_task_runner_;
  DCHECK(runner);
  if (!runner->RunsTasksInCurrentSequence())
    runner->DeleteSoon(FROM_HERE, std::move(self));
}

void VideoEncodeAcceleratorAdapter::SetInputBufferPreferenceForTesting(
    InputBufferKind pref) {
  input_buffer_preference_ = pref;
}

void VideoEncodeAcceleratorAdapter::Initialize(VideoCodecProfile profile,
                                               const Options& options,
                                               EncoderInfoCB info_cb,
                                               OutputCB output_cb,
                                               EncoderStatusCB done_cb) {
  DCHECK(!accelerator_task_runner_->RunsTasksInCurrentSequence());
  accelerator_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &VideoEncodeAcceleratorAdapter::InitializeOnAcceleratorThread,
          base::Unretained(this), profile, options,
          WrapCallback(std::move(info_cb)), WrapCallback(std::move(output_cb)),
          WrapCallback(std::move(done_cb))));
}

void VideoEncodeAcceleratorAdapter::InitializeOnAcceleratorThread(
    VideoCodecProfile profile,
    const Options& options,
    EncoderInfoCB info_cb,
    OutputCB output_cb,
    EncoderStatusCB done_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(accelerator_sequence_checker_);
  if (state_ != State::kNotInitialized) {
    std::move(done_cb).Run(
        EncoderStatus(EncoderStatus::Codes::kEncoderInitializeTwice,
                      "Encoder has already been initialized."));
    return;
  }

  accelerator_ = gpu_factories_->CreateVideoEncodeAccelerator();
  if (!accelerator_) {
    std::move(done_cb).Run(
        EncoderStatus(EncoderStatus::Codes::kEncoderInitializationError,
                      "Failed to create video encode accelerator."));
    return;
  }

  if (options.frame_size.width() <= 0 || options.frame_size.height() <= 0) {
    std::move(done_cb).Run(
        EncoderStatus(EncoderStatus::Codes::kEncoderUnsupportedConfig,
                      "Negative width or height values."));
    return;
  }

  if (!options.frame_size.GetCheckedArea().IsValid()) {
    std::move(done_cb).Run(
        EncoderStatus(EncoderStatus::Codes::kEncoderUnsupportedConfig,
                      "Frame is too large."));
    return;
  }

  auto supported_profiles =
      gpu_factories_->GetVideoEncodeAcceleratorSupportedProfiles();
  if (!supported_profiles) {
    InitCompleted(
        EncoderStatus(EncoderStatus::Codes::kEncoderUnsupportedProfile,
                      "No profile is supported by video encode accelerator."));
    return;
  }

  auto supported_rc_modes =
      VideoEncodeAccelerator::SupportedRateControlMode::kNoMode;
  std::vector<VideoPixelFormat> gpu_supported_pixel_formats;
  for (const auto& supported_profile : *supported_profiles) {
    if (supported_profile.profile == profile) {
      supported_rc_modes = supported_profile.rate_control_modes;
      gpu_supported_pixel_formats =
          supported_profile.gpu_supported_pixel_formats;
      break;
    }
  }

  if (supported_rc_modes ==
      VideoEncodeAccelerator::SupportedRateControlMode::kNoMode) {
    std::move(done_cb).Run(
        EncoderStatus(
            EncoderStatus::Codes::kEncoderUnsupportedProfile,
            "The profile is not supported by video encode accelerator.")
            .WithData("profile", profile));
    return;
  }

  profile_ = profile;
  supported_rc_modes_ = supported_rc_modes;
  gpu_supported_pixel_formats_ = std::move(gpu_supported_pixel_formats);
  options_ = options;
  info_cb_ = std::move(info_cb);
  output_cb_ = std::move(output_cb);
  state_ = State::kWaitingForFirstFrame;

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  if (profile_ >= H264PROFILE_MIN && profile_ <= H264PROFILE_MAX &&
      !options_.avc.produce_annexb) {
    h264_converter_ = std::make_unique<H264AnnexBToAvcBitstreamConverter>();
  }
#if BUILDFLAG(ENABLE_PLATFORM_HEVC) && \
    BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
  if (profile_ == HEVCPROFILE_MAIN && !options_.hevc.produce_annexb) {
    h265_converter_ = std::make_unique<H265AnnexBToHevcBitstreamConverter>();
  }
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC) &&
        // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

  std::move(done_cb).Run(EncoderStatus::Codes::kOk);

  // The accelerator will be initialized for real once we have the first frame.
}

void VideoEncodeAcceleratorAdapter::InitializeInternalOnAcceleratorThread() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(accelerator_sequence_checker_);
  DCHECK_EQ(state_, State::kWaitingForFirstFrame);
  DCHECK(!pending_encodes_.empty());

  // We use the first frame to setup the VEA config so that we can ensure that
  // zero copy hardware encoding from the camera can be used.
  const auto& first_frame = pending_encodes_.front()->frame;
  const auto format = first_frame->format();
  const bool is_rgb =
      format == PIXEL_FORMAT_XBGR || format == PIXEL_FORMAT_XRGB ||
      format == PIXEL_FORMAT_ABGR || format == PIXEL_FORMAT_ARGB;
  const bool supported_format =
      format == PIXEL_FORMAT_NV12 || format == PIXEL_FORMAT_I420 || is_rgb;
  const bool is_gpu_frame =
      first_frame->HasSharedImage() || first_frame->HasNativeGpuMemoryBuffer();
  const bool is_gpu_supported_format =
      is_gpu_frame &&
      base::ranges::find(gpu_supported_pixel_formats_, format) !=
          gpu_supported_pixel_formats_.end();
  if (!supported_format && !is_gpu_supported_format) {
    InitCompleted(EncoderStatus(EncoderStatus::Codes::kUnsupportedFrameFormat,
                                "Unexpected frame format.")
                      .WithData("frame", first_frame->AsHumanReadableString()));
    return;
  }

  auto vea_config = SetUpVeaConfig(
      profile_, options_, format, first_frame->storage_type(),
      supported_rc_modes_, required_encoder_type_, is_gpu_supported_format);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // Linux/ChromeOS require a special configuration to use dmabuf storage.
  // We need to keep sending frames the same way the first frame was sent.
  // Other platforms will happily mix GpuMemoryBuffer storage with regular
  // storage, so we don't care about mismatches on other platforms.
  if (input_buffer_preference_ == InputBufferKind::Any) {
    if (vea_config.storage_type ==
        VideoEncodeAccelerator::Config::StorageType::kGpuMemoryBuffer) {
      input_buffer_preference_ = InputBufferKind::GpuMemBuf;
    } else {
      input_buffer_preference_ = InputBufferKind::CpuMemBuf;
    }
  }
#endif
  if (!accelerator_->Initialize(vea_config, this, media_log_->Clone())) {
    InitCompleted(
        EncoderStatus(EncoderStatus::Codes::kEncoderInitializationError,
                      "Failed to initialize video encode accelerator."));
    return;
  }

  state_ = State::kInitializing;
  format_ = vea_config.input_format;
}

void VideoEncodeAcceleratorAdapter::Encode(scoped_refptr<VideoFrame> frame,
                                           const EncodeOptions& encode_options,
                                           EncoderStatusCB done_cb) {
  DCHECK(!accelerator_task_runner_->RunsTasksInCurrentSequence());
  accelerator_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoEncodeAcceleratorAdapter::EncodeOnAcceleratorThread,
                     base::Unretained(this), std::move(frame), encode_options,
                     WrapCallback(std::move(done_cb))));
}

void VideoEncodeAcceleratorAdapter::EncodeOnAcceleratorThread(
    scoped_refptr<VideoFrame> frame,
    EncodeOptions encode_options,
    EncoderStatusCB done_cb) {
  TRACE_EVENT1("media",
               "VideoEncodeAcceleratorAdapter::EncodeOnAcceleratorThread",
               "timestamp", frame->timestamp());
  DCHECK_CALLED_ON_VALID_SEQUENCE(accelerator_sequence_checker_);

  if (state_ == State::kWaitingForFirstFrame ||
      state_ == State::kInitializing) {
    auto pending_encode = std::make_unique<PendingEncode>();
    pending_encode->done_callback = std::move(done_cb);
    pending_encode->frame = std::move(frame);
    pending_encode->options = encode_options;
    pending_encodes_.push_back(std::move(pending_encode));
    if (state_ == State::kWaitingForFirstFrame)
      InitializeInternalOnAcceleratorThread();
    return;
  }

  if (state_ != State::kReadyToEncode) {
    std::move(done_cb).Run(
        EncoderStatus(EncoderStatus::Codes::kEncoderIllegalState,
                      "Encoder can't encode now.")
            .WithData("state", static_cast<int32_t>(state_)));
    return;
  }

  const bool frame_needs_resizing =
      frame->visible_rect().size() != options_.frame_size ||
      frame->coded_size() != input_coded_size_;

  // Try using a frame with GPU buffer both are true:
  // 1. the frame already has GPU buffer
  // 2. frame doesn't need resizing or can be resized by GPU encoder.
  bool use_gpu_buffer = frame->HasMappableGpuBuffer() &&
                        (!frame_needs_resizing || gpu_resize_supported_);

  // Currently configured encoder's preference takes precedence overe heuristic
  // above.
  if (input_buffer_preference_ == InputBufferKind::GpuMemBuf)
    use_gpu_buffer = true;
  if (input_buffer_preference_ == InputBufferKind::CpuMemBuf)
    use_gpu_buffer = false;

  EncoderStatus::Or<scoped_refptr<VideoFrame>> result(nullptr);
  if (use_gpu_buffer)
    result = PrepareGpuFrame(frame);
  else
    result = PrepareCpuFrame(frame);

  if (!result.has_value()) {
    std::move(done_cb).Run(
        std::move(result)
            .error()
            .WithData("frame", frame->AsHumanReadableString())
            .AddHere());
    return;
  }

  frame = std::move(result).value();

  if (last_frame_color_space_ != frame->ColorSpace()) {
    last_frame_color_space_ = frame->ColorSpace();
    encode_options.key_frame = true;
  }

  auto active_encode = std::make_unique<PendingOp>();
  active_encode->done_callback = std::move(done_cb);
  active_encode->timestamp = frame->timestamp();
  active_encode->color_space = frame->ColorSpace();
  active_encodes_.push_back(std::move(active_encode));
  accelerator_->Encode(frame, encode_options);
}

void VideoEncodeAcceleratorAdapter::ChangeOptions(const Options& options,
                                                  OutputCB output_cb,
                                                  EncoderStatusCB done_cb) {
  DCHECK(!accelerator_task_runner_->RunsTasksInCurrentSequence());
  accelerator_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &VideoEncodeAcceleratorAdapter::ChangeOptionsOnAcceleratorThread,
          base::Unretained(this), options, WrapCallback(std::move(output_cb)),
          WrapCallback(std::move(done_cb))));
}

void VideoEncodeAcceleratorAdapter::ChangeOptionsOnAcceleratorThread(
    const Options options,
    OutputCB output_cb,
    EncoderStatusCB done_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(accelerator_sequence_checker_);
  DCHECK(active_encodes_.empty());
  DCHECK(pending_encodes_.empty());
  CHECK(state_ == State::kReadyToEncode ||
        state_ == State::kWaitingForFirstFrame);

  if (options.bitrate && options_.bitrate &&
      options.bitrate->mode() != options_.bitrate->mode()) {
    std::move(done_cb).Run(
        EncoderStatus(EncoderStatus::Codes::kEncoderUnsupportedConfig,
                      "Bitrate mode change is not supported.")
            .WithData("current bitrate",
                      static_cast<int32_t>(options_.bitrate->mode()))
            .WithData("new bitrate",
                      static_cast<int32_t>(options.bitrate->mode())));
    return;
  }

  std::optional<gfx::Size> new_frame_size;
  if (options.frame_size != options_.frame_size) {
    if (supports_frame_size_change_) {
      input_pool_.reset();
      gmb_frame_pool_.reset();
      new_frame_size = options.frame_size;
    } else {
      auto status =
          EncoderStatus(EncoderStatus::Codes::kEncoderUnsupportedConfig,
                        "Resolution change is not supported.");
      std::move(done_cb).Run(status);
      return;
    }
  }

  Bitrate bitrate =
      CreateBitrate(options.bitrate, options.frame_size, supported_rc_modes_);
  uint32_t framerate = base::ClampRound<uint32_t>(
      options.framerate.value_or(VideoEncodeAccelerator::kDefaultFramerate));

  // When frame size is changed, run |done_cb| in |RequireBitstreamBuffers|
  // after bitstream buffer is re-initialized. At that time, reconfigure is done
  // and VEA is ready to accept new frames.
  if (new_frame_size.has_value()) {
    reconfigure_cb_ = std::move(done_cb);
  }
  state_ = State::kReconfiguring;
  accelerator_->RequestEncodingParametersChange(bitrate, framerate,
                                                new_frame_size);

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  if (profile_ >= H264PROFILE_MIN && profile_ <= H264PROFILE_MAX) {
    if (options.avc.produce_annexb) {
      h264_converter_.reset();
    } else if (!h264_converter_) {
      h264_converter_ = std::make_unique<H264AnnexBToAvcBitstreamConverter>();
    }
  }
#if BUILDFLAG(ENABLE_PLATFORM_HEVC) && \
    BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
  if (profile_ == HEVCPROFILE_MAIN) {
    if (options.hevc.produce_annexb) {
      h265_converter_.reset();
    } else if (!h265_converter_) {
      h265_converter_ = std::make_unique<H265AnnexBToHevcBitstreamConverter>();
    }
  }
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC) &&
        // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

  options_ = options;
  if (!output_cb.is_null())
    output_cb_ = std::move(output_cb);
  if (!done_cb.is_null()) {
    state_ = State::kReadyToEncode;
    std::move(done_cb).Run(EncoderStatus::Codes::kOk);
  }
}

void VideoEncodeAcceleratorAdapter::Flush(EncoderStatusCB done_cb) {
  DCHECK(!accelerator_task_runner_->RunsTasksInCurrentSequence());
  accelerator_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoEncodeAcceleratorAdapter::FlushOnAcceleratorThread,
                     base::Unretained(this), WrapCallback(std::move(done_cb))));
}

void VideoEncodeAcceleratorAdapter::FlushOnAcceleratorThread(
    EncoderStatusCB done_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(accelerator_sequence_checker_);
  if (state_ == State::kWaitingForFirstFrame) {
    // Nothing to do since we haven't actually initialized yet.
    std::move(done_cb).Run(EncoderStatus::Codes::kOk);
    return;
  }

  if (state_ != State::kReadyToEncode && state_ != State::kInitializing) {
    std::move(done_cb).Run(EncoderStatus(
        EncoderStatus::Codes::kEncoderFailedFlush, "Encoder can't flush now"));
    return;
  }

  if (active_encodes_.empty() && pending_encodes_.empty()) {
    // No active or pending encodes, nothing to flush.
    std::move(done_cb).Run(EncoderStatus::Codes::kOk);
    return;
  }

  // When initializing the flush will be handled after pending encodes are sent.
  if (state_ != State::kInitializing) {
    DCHECK_EQ(state_, State::kReadyToEncode);
    state_ = State::kFlushing;
  }

  pending_flush_ = std::make_unique<PendingOp>();
  pending_flush_->done_callback = std::move(done_cb);

  // If flush is not supported FlushCompleted() will be called by
  // BitstreamBufferReady() when |active_encodes_| is empty.
  if (state_ == State::kFlushing && flush_support_.value()) {
    accelerator_->Flush(
        base::BindOnce(&VideoEncodeAcceleratorAdapter::FlushCompleted,
                       base::Unretained(this)));
  }
}

void VideoEncodeAcceleratorAdapter::RequireBitstreamBuffers(
    unsigned int input_count,
    const gfx::Size& input_coded_size,
    size_t output_buffer_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(accelerator_sequence_checker_);
  CHECK(state_ == State::kInitializing || state_ == State::kReconfiguring);

  input_coded_size_ = input_coded_size;

  constexpr int kOutputBufferNumber = 2;
  output_buffer_handles_.clear();
  for (int id = 0; id < kOutputBufferNumber; id++) {
    auto handle = output_pool_->MaybeAllocateBuffer(output_buffer_size);
    if (!handle) {
      if (state_ == State::kInitializing) {
        InitCompleted(EncoderStatus::Codes::kOutOfMemoryError);
      } else if (reconfigure_cb_) {
        std::move(reconfigure_cb_)
            .Run({EncoderStatus::Codes::kOutOfMemoryError,
                  "Failed to allocate buffer"});
        state_ = State::kNotInitialized;
      }
      return;
    }

    const base::UnsafeSharedMemoryRegion& region = handle->GetRegion();
    accelerator_->UseOutputBitstreamBuffer(
        BitstreamBuffer(id, region.Duplicate(), region.GetSize()));
    output_buffer_handles_.push_back(std::move(handle));
  }

  if (state_ == State::kInitializing) {
    InitCompleted(EncoderStatus::Codes::kOk);
  } else if (state_ == State::kReconfiguring) {
    state_ = State::kReadyToEncode;
    if (reconfigure_cb_) {
      std::move(reconfigure_cb_).Run(EncoderStatus::Codes::kOk);
    }
  }
}

void VideoEncodeAcceleratorAdapter::BitstreamBufferReady(
    int32_t buffer_id,
    const BitstreamBufferMetadata& metadata) {
  std::optional<CodecDescription> desc;
  VideoEncoderOutput result;
  result.key_frame = metadata.key_frame;
  result.timestamp = metadata.timestamp;
  if (metadata.h264.has_value())
    result.temporal_id = metadata.h264.value().temporal_idx;
  else if (metadata.vp9.has_value())
    result.temporal_id = metadata.vp9.value().temporal_idx;
  else if (metadata.vp8.has_value())
    result.temporal_id = metadata.vp8.value().temporal_idx;
  else if (metadata.av1.has_value())
    result.temporal_id = metadata.av1.value().temporal_idx;
  else if (metadata.h265.has_value())
    result.temporal_id = metadata.h265.value().temporal_idx;

  if (metadata.encoded_size)
    result.encoded_size = metadata.encoded_size;

  if (buffer_id < 0 ||
      buffer_id >= static_cast<int>(output_buffer_handles_.size())) {
    NotifyErrorStatus(
        {EncoderStatus::Codes::kInvalidOutputBuffer,
         "Buffer id is out of bounds: " + base::NumberToString(buffer_id)});
  }
  if (!output_buffer_handles_[buffer_id]) {
    NotifyErrorStatus(
        {EncoderStatus::Codes::kInvalidOutputBuffer, "Invalid output buffer"});
  }

  const base::WritableSharedMemoryMapping& mapping =
      output_buffer_handles_[buffer_id]->GetMapping();
  DCHECK_LE(metadata.payload_size_bytes, mapping.size());

  if (metadata.payload_size_bytes > 0) {
    bool stream_converted = false;
    auto src = mapping.GetMemoryAsSpan<uint8_t>(metadata.payload_size_bytes);

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    size_t actual_output_size = 0;
    auto dst = base::HeapArray<uint8_t>::Uninit(metadata.payload_size_bytes);
    bool config_changed = false;
    media::MP4Status status;
    if (h264_converter_) {
      do {
        status = h264_converter_->ConvertChunk(src, dst, &config_changed,
                                               &actual_output_size);
        if (status.code() == MP4Status::Codes::kBufferTooSmall) {
          // Between AnnexB and AVCC bitstream formats, the start code length
          // and the nal size length can be different. See H.264 specification
          // at http://www.itu.int/rec/T-REC-H.264. Retry the conversion if the
          // output buffer size is too small.
          dst = base::HeapArray<uint8_t>::Uninit(actual_output_size);
          continue;
        } else if (!status.is_ok()) {
          NotifyErrorStatus(
              EncoderStatus(EncoderStatus::Codes::kBitstreamConversionError,
                            "Failed to convert a buffer to h264 chunk")
                  .AddCause(std::move(status)));
          return;
        }
      } while (!status.is_ok());
      result.data = std::move(dst).take_first(actual_output_size);
      stream_converted = true;

      if (config_changed) {
        const auto& config = h264_converter_->GetCurrentConfig();
        desc = CodecDescription();
        if (!config.Serialize(desc.value())) {
          NotifyErrorStatus(
              {media::EncoderStatus::Codes::kBitstreamConversionError,
               "Failed to get h264 config"});
          return;
        }
      }
    } else {
#if BUILDFLAG(ENABLE_PLATFORM_HEVC) && \
    BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
      if (h265_converter_) {
        do {
          status = h265_converter_->ConvertChunk(src, dst, &config_changed,
                                                 &actual_output_size);
          if (status.code() == MP4Status::Codes::kBufferTooSmall) {
            dst = base::HeapArray<uint8_t>::Uninit(actual_output_size);
            continue;
          } else if (!status.is_ok()) {
            NotifyErrorStatus(
                EncoderStatus(EncoderStatus::Codes::kBitstreamConversionError,
                              "Failed to convert a buffer to h265 chunk")
                    .AddCause(std::move(status)));
            return;
          }
        } while (!status.is_ok());
        result.data = std::move(dst).take_first(actual_output_size);
        stream_converted = true;

        if (config_changed) {
          const auto& config = h265_converter_->GetCurrentConfig();
          desc = CodecDescription();
          if (!config.Serialize(desc.value())) {
            NotifyErrorStatus(
                {media::EncoderStatus::Codes::kBitstreamConversionError,
                 "Failed to get h265 config"});
            return;
          }
        }
      }
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC) &&
        // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
    }
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
    if (!stream_converted) {
      result.data = base::HeapArray<uint8_t>::CopiedFrom(src);
    }
  }

  // Give the buffer back to |accelerator_|
  const base::UnsafeSharedMemoryRegion& region =
      output_buffer_handles_[buffer_id]->GetRegion();
  accelerator_->UseOutputBitstreamBuffer(
      BitstreamBuffer(buffer_id, region.Duplicate(), region.GetSize()));

  bool erased_active_encode = false;
  for (auto it = active_encodes_.begin(); it != active_encodes_.end(); ++it) {
    if ((*it)->timestamp == result.timestamp) {
      result.color_space =
          metadata.encoded_color_space.value_or((*it)->color_space);
      std::move((*it)->done_callback).Run(EncoderStatus::Codes::kOk);
      active_encodes_.erase(it);
      erased_active_encode = true;
      break;
    }
  }
  DCHECK(erased_active_encode);
  output_cb_.Run(std::move(result), std::move(desc));

  if (active_encodes_.empty() && !flush_support_.value()) {
    // Manually call FlushCompleted(), since |accelerator_| won't do it for us.
    FlushCompleted(true);
  }
}

void VideoEncodeAcceleratorAdapter::NotifyErrorStatus(
    const EncoderStatus& status) {
  CHECK(!status.is_ok());
  MEDIA_LOG(ERROR, media_log_)
      << "VEA adapter error. Code: " << static_cast<int32_t>(status.code())
      << ". Message: " << status.message();
  if (state_ == State::kInitializing) {
    InitCompleted(status);
    return;
  }

  if (state_ == State::kReconfiguring) {
    if (reconfigure_cb_) {
      std::move(reconfigure_cb_).Run(status);
    }
    state_ = State::kNotInitialized;
    return;
  }

  if (state_ == State::kFlushing)
    FlushCompleted(false);

  // Report the error to all encoding-done callbacks
  for (auto& encode : active_encodes_) {
    std::move(encode->done_callback).Run(status);
  }
  active_encodes_.clear();
  state_ = State::kNotInitialized;
}

void VideoEncodeAcceleratorAdapter::NotifyEncoderInfoChange(
    const VideoEncoderInfo& info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(accelerator_sequence_checker_);
  supports_frame_size_change_ = info.supports_frame_size_change;
  if (info_cb_) {
    info_cb_.Run(info);
  }
}

void VideoEncodeAcceleratorAdapter::InitCompleted(EncoderStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(accelerator_sequence_checker_);

  if (!status.is_ok()) {
    // Report the error to all encoding-done callbacks
    for (auto& encode : pending_encodes_)
      std::move(encode->done_callback).Run(status);

    if (pending_flush_)
      FlushCompleted(false);

    DCHECK(active_encodes_.empty());
    pending_encodes_.clear();
    state_ = State::kNotInitialized;
    return;
  }

  state_ = State::kReadyToEncode;
  flush_support_ = accelerator_->IsFlushSupported();
  gpu_resize_supported_ = accelerator_->IsGpuFrameResizeSupported();

  // Send off the encodes that came in while we were waiting for initialization.
  for (auto& encode : pending_encodes_) {
    EncodeOnAcceleratorThread(std::move(encode->frame), encode->options,
                              std::move(encode->done_callback));
  }
  pending_encodes_.clear();

  // If a Flush() came in during initialization, transition to flushing now that
  // all the pending encodes have been sent.
  if (pending_flush_) {
    state_ = State::kFlushing;
    if (flush_support_.value()) {
      accelerator_->Flush(
          base::BindOnce(&VideoEncodeAcceleratorAdapter::FlushCompleted,
                         base::Unretained(this)));
    }
  }
}

void VideoEncodeAcceleratorAdapter::FlushCompleted(bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(accelerator_sequence_checker_);
  if (!pending_flush_)
    return;

  std::move(pending_flush_->done_callback)
      .Run(success ? EncoderStatus::Codes::kOk
                   : EncoderStatus::Codes::kEncoderFailedFlush);
  pending_flush_.reset();
  state_ = State::kReadyToEncode;
}

template <class T>
T VideoEncodeAcceleratorAdapter::WrapCallback(T cb) {
  DCHECK(callback_task_runner_);
  if (cb.is_null())
    return cb;
  return base::BindPostTask(callback_task_runner_, std::move(cb));
}

// Copy a frame into a shared mem buffer and resize it as the same time. Input
// frames can I420, NV12, or RGB -- they'll be converted to I420 if needed.
EncoderStatus::Or<scoped_refptr<VideoFrame>>
VideoEncodeAcceleratorAdapter::PrepareCpuFrame(
    scoped_refptr<VideoFrame> src_frame) {
  TRACE_EVENT0("media", "VideoEncodeAcceleratorAdapter::PrepareCpuFrame");

  const auto dest_coded_size = input_coded_size_;
  const auto dest_visible_rect = gfx::Rect(options_.frame_size);

  // The frame whose storage type is STORAGE_OWNED_MEMORY and
  // STORAGE_UNOWNED_MEMORY is copied here, not in mojo_video_frame_traits.
  // It is because VEAAdapter recycles the SharedMemoryRegion, but
  // mojo_video_frame_traits doesn't.
  if (src_frame->storage_type() == VideoFrame::STORAGE_SHMEM &&
      src_frame->format() == PIXEL_FORMAT_I420 &&
      src_frame->visible_rect() == dest_visible_rect &&
      src_frame->coded_size() == dest_coded_size) {
    // Nothing to do here, the input frame is already what we need.
    return src_frame;
  }

  if (!input_pool_) {
    const size_t input_buffer_size =
        VideoFrame::AllocationSize(PIXEL_FORMAT_I420, dest_coded_size);
    input_pool_ = base::MakeRefCounted<ReadOnlyRegionPool>(input_buffer_size);
  }

  std::unique_ptr<ReadOnlyRegionPool::Handle> handle =
      input_pool_->MaybeAllocateBuffer();
  if (!handle || !handle->IsValid())
    return EncoderStatus(EncoderStatus::Codes::kOutOfMemoryError);

  const base::WritableSharedMemoryMapping* mapping = handle->mapping();
  auto mapped_src_frame = src_frame->HasMappableGpuBuffer()
                              ? ConvertToMemoryMappedFrame(src_frame)
                              : src_frame;
  auto shared_frame = VideoFrame::WrapExternalData(
      PIXEL_FORMAT_I420, dest_coded_size, dest_visible_rect,
      dest_visible_rect.size(), static_cast<uint8_t*>(mapping->memory()),
      mapping->size(), src_frame->timestamp());

  if (!shared_frame || !mapped_src_frame)
    return EncoderStatus(EncoderStatus::Codes::kSystemAPICallError);

  auto status =
      frame_converter_.ConvertAndScale(*mapped_src_frame, *shared_frame);
  if (!status.is_ok()) {
    return status;
  }

  shared_frame->BackWithSharedMemory(handle->region());
  shared_frame->AddDestructionObserver(
      base::DoNothingWithBoundArgs(std::move(handle)));
  return shared_frame;
}

// Copy a frame into a GPU buffer and resize it as the same time. Input frames
// can I420, NV12, or RGB -- they'll be converted to NV12 if needed.
EncoderStatus::Or<scoped_refptr<VideoFrame>>
VideoEncodeAcceleratorAdapter::PrepareGpuFrame(
    scoped_refptr<VideoFrame> src_frame) {
  TRACE_EVENT0("media", "VideoEncodeAcceleratorAdapter::PrepareGpuFrame");
  DCHECK_CALLED_ON_VALID_SEQUENCE(accelerator_sequence_checker_);
  DCHECK(src_frame);

  const auto dest_coded_size = input_coded_size_;
  const auto dest_visible_rect = gfx::Rect(options_.frame_size);
  bool is_gpu_supported_format =
      base::ranges::find(gpu_supported_pixel_formats_, src_frame->format()) !=
      gpu_supported_pixel_formats_.end();

  if (src_frame->HasMappableGpuBuffer() &&
      (src_frame->format() == PIXEL_FORMAT_NV12 || is_gpu_supported_format) &&
      (gpu_resize_supported_ || src_frame->coded_size() == dest_coded_size)) {
    // Nothing to do here, the input frame is already what we need
    return src_frame;
  }

  if (!gmb_frame_pool_) {
    gmb_frame_pool_ = base::MakeRefCounted<GpuMemoryBufferVideoFramePool>(
        gpu_factories_, dest_coded_size);
  }

  auto gpu_frame =
      gmb_frame_pool_->MaybeCreateVideoFrame(dest_visible_rect.size());
  if (!gpu_frame)
    return EncoderStatus(EncoderStatus::Codes::kOutOfMemoryError);

  gpu_frame->set_timestamp(src_frame->timestamp());
  gpu_frame->metadata().MergeMetadataFrom(src_frame->metadata());

  // Don't be scared. ConvertToMemoryMappedFrame() doesn't copy pixel data
  // it just maps GPU buffer owned by |gpu_frame| and presents it as mapped
  // view in CPU memory. It allows us to use ConvertAndScale() without
  // having to tinker with libyuv and GpuMemoryBuffer memory views.
  // |mapped_gpu_frame| doesn't own anything, but unmaps the buffer when freed.
  // This is true because |gpu_frame| is created with
  // |VEA_READ_CAMERA_AND_CPU_READ_WRITE| usage flag.
  auto mapped_gpu_frame = ConvertToMemoryMappedFrame(gpu_frame);
  auto mapped_src_frame = src_frame->HasMappableGpuBuffer()
                              ? ConvertToMemoryMappedFrame(src_frame)
                              : src_frame;
  if (!mapped_gpu_frame || !mapped_src_frame)
    return EncoderStatus(EncoderStatus::Codes::kSystemAPICallError);

  auto status =
      frame_converter_.ConvertAndScale(*mapped_src_frame, *mapped_gpu_frame);
  if (!status.is_ok()) {
    return status;
  }

  // |mapped_gpu_frame| has the color space respecting the color conversion in
  // ConvertAndScale().
#if BUILDFLAG(IS_MAC)
  gpu_frame->shared_image()->SetColorSpaceOnNativeBuffer(
      mapped_gpu_frame->ColorSpace());
#endif
  gpu_frame->set_color_space(mapped_gpu_frame->ColorSpace());

  return gpu_frame;
}

}  // namespace media

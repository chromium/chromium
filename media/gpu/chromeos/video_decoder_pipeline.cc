// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/video_decoder_pipeline.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "media/base/async_destroy_video_decoder.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/media_util.h"
#include "media/gpu/chromeos/dmabuf_video_frame_pool.h"
#include "media/gpu/chromeos/image_processor.h"
#include "media/gpu/chromeos/image_processor_factory.h"
#include "media/gpu/chromeos/native_pixmap_frame_resource.h"
#include "media/gpu/chromeos/oop_video_decoder.h"
#include "media/gpu/chromeos/platform_video_frame_pool.h"
#include "media/gpu/chromeos/video_frame_resource.h"
#include "media/gpu/macros.h"
#include "media/media_buildflags.h"

#if BUILDFLAG(USE_VAAPI)
#include <drm_fourcc.h>
#include "media/gpu/vaapi/vaapi_video_decoder.h"
#elif BUILDFLAG(USE_V4L2_CODEC)
#include "media/gpu/v4l2/v4l2_stateful_video_decoder.h"
#include "media/gpu/v4l2/v4l2_video_decoder.h"
#else
#error Either VA-API or V4L2 must be used for decode acceleration on Chrome OS.
#endif

namespace media {
namespace {

using PixelLayoutCandidate = ImageProcessor::PixelLayoutCandidate;

// Picks the preferred compositor renderable format from |candidates|, if any.
// If |preferred_fourcc| is provided, contained in |candidates|, and considered
// renderable, it returns that. Otherwise, it goes through
// |renderable_fourccs| until it finds one that's in |candidates|. If
// it can't find a renderable format in |candidates|, it returns std::nullopt.
std::optional<Fourcc> PickRenderableFourcc(
    const std::vector<Fourcc>& renderable_fourccs,
    const std::vector<Fourcc>& candidates,
    std::optional<Fourcc> preferred_fourcc) {
  if (preferred_fourcc && base::Contains(candidates, *preferred_fourcc) &&
      base::Contains(renderable_fourccs, *preferred_fourcc)) {
    return preferred_fourcc;
  }
  for (const auto& value : renderable_fourccs) {
    if (base::Contains(candidates, value))
      return value;
  }
  return std::nullopt;
}

// Estimates the number of buffers needed in the output frame pool to fill the
// Renderer pipeline (this pool may provide buffers to the VideoDecoder
// directly or to the ImageProcessor, when this is instantiated).
size_t EstimateRequiredRendererPipelineBuffers(bool low_delay,
                                               bool use_protected) {
  // kMaxVideoFrames is meant to be the number of VideoFrames needed to populate
  // the whole Renderer playback pipeline when there's no smoothing playback
  // queue, i.e. in low latency scenarios such as WebRTC etc. For non-low
  // latency scenarios, a large smoothing playback is used in the Renderer
  // process. Heuristically, the extra depth needed is in the range of 15 or
  // so, so we need to add a few extra buffers.
  // For V4L2 secure playback, we need to limit the pipeline depth or we will
  // run out of memory. We are going further than we are with the test because
  // the memory consequences of using more are too great and so far have yielded
  // no problems in testing.
  constexpr size_t kExpectedNonLatencyPipelineDepth = 16;
#if BUILDFLAG(USE_V4L2_CODEC)
  constexpr size_t kExpectedNonLatencyPipelineDepthSecure = 6;
#endif
  static_assert(kExpectedNonLatencyPipelineDepth > limits::kMaxVideoFrames,
                "kMaxVideoFrames is expected to be relatively small");
  constexpr size_t kReducedNonLatencyPipelineDepth = 8;

  if (low_delay) {
    return limits::kMaxVideoFrames + 1;
#if BUILDFLAG(USE_V4L2_CODEC)
  } else if (use_protected) {
    return kExpectedNonLatencyPipelineDepthSecure;
#endif
  } else if (base::FeatureList::IsEnabled(kReduceHardwareVideoDecoderBuffers)) {
    return kReducedNonLatencyPipelineDepth;
  } else {
    return kExpectedNonLatencyPipelineDepth;
  }
}

scoped_refptr<base::SequencedTaskRunner> GetDecoderTaskRunner(
    bool in_video_decoder_process) {
  // Note that the decoder thread is created with base::MayBlock(). This is
  // because the underlying |decoder_| may need to allocate a dummy buffer
  // to discover the most native modifier accepted by the hardware video
  // decoder; this in turn may need to open the render node, and this is the
  // operation that may block.
  if (in_video_decoder_process) {
    return base::ThreadPool::CreateSequencedTaskRunner(
        {base::TaskPriority::USER_VISIBLE, base::MayBlock()});
  }
  return base::ThreadPool::CreateSingleThreadTaskRunner(
      {base::WithBaseSyncPrimitives(), base::TaskPriority::USER_VISIBLE,
       base::MayBlock()},
      base::SingleThreadTaskRunnerThreadMode::DEDICATED);
}

// DefaultFrameConverter uses the FrameResource built-in converters to handle
// conversion to VideoFrame objects. It is used by VideoDecoderPipeline when a
// client doesn't specify a FrameConverter.
class DefaultFrameConverter : public FrameResourceConverter {
 public:
  static std::unique_ptr<FrameResourceConverter> Create() {
    return base::WrapUnique<FrameResourceConverter>(
        new DefaultFrameConverter());
  }

  DefaultFrameConverter(const DefaultFrameConverter&) = delete;
  DefaultFrameConverter& operator=(const DefaultFrameConverter&) = delete;

 private:
  DefaultFrameConverter() = default;
  ~DefaultFrameConverter() override = default;

  // FrameConverter overrides.
  void ConvertFrameImpl(scoped_refptr<FrameResource> frame) override {
    DVLOGF(4);

    if (!frame) {
      return OnError(FROM_HERE, "Invalid frame.");
    }
    LOG_ASSERT(frame->AsVideoFrameResource() ||
               frame->AsNativePixmapFrameResource())
        << "|frame| is expected to be a VideoFrameResource or "
           "NativePixmapFrameResource";
    scoped_refptr<VideoFrame> video_frame =
        frame->AsVideoFrameResource()
            ? frame->AsVideoFrameResource()->GetMutableVideoFrame()
            : frame->AsNativePixmapFrameResource()->CreateVideoFrame();
    if (!video_frame) {
      return OnError(FROM_HERE,
                     "Failed to convert FrameResource to VideoFrame.");
    }
    Output(std::move(video_frame));
  }
};
}  //  namespace

VideoDecoderMixin::VideoDecoderMixin(
    std::unique_ptr<MediaLog> media_log,
    scoped_refptr<base::SequencedTaskRunner> decoder_task_runner,
    base::WeakPtr<VideoDecoderMixin::Client> client)
    : media_log_(std::move(media_log)),
      decoder_task_runner_(std::move(decoder_task_runner)),
      client_(std::move(client)) {}

VideoDecoderMixin::~VideoDecoderMixin() = default;

bool VideoDecoderMixin::NeedsTranscryption() {
  return false;
}

CroStatus VideoDecoderMixin::AttachSecureBuffer(
    scoped_refptr<DecoderBuffer>& buffer) {
  return CroStatus::Codes::kOk;
}

void VideoDecoderMixin::Initialize(const VideoDecoderConfig& config,
                                   bool low_delay,
                                   CdmContext* cdm_context,
                                   InitCB init_cb,
                                   const OutputCB& output_cb,
                                   const WaitingCB& waiting_cb) {
  NOTREACHED_IN_MIGRATION()
      << "FrameResource version of Initialize is used instead";
}

void VideoDecoderMixin::ReleaseSecureBuffer(uint64_t secure_handle) {}

size_t VideoDecoderMixin::GetMaxOutputFramePoolSize() const {
  return std::numeric_limits<size_t>::max();
}

VideoDecoderPipeline::ClientFlushCBState::ClientFlushCBState(
    DecodeCB flush_cb,
    DecoderStatus decoder_decode_status)
    : flush_cb(std::move(flush_cb)),
      decoder_decode_status(decoder_decode_status) {}

VideoDecoderPipeline::ClientFlushCBState::~ClientFlushCBState() = default;

// static
std::unique_ptr<VideoDecoder> VideoDecoderPipeline::Create(
    const gpu::GpuDriverBugWorkarounds& workarounds,
    scoped_refptr<base::SequencedTaskRunner> client_task_runner,
    std::unique_ptr<DmabufVideoFramePool> frame_pool,
    std::unique_ptr<FrameResourceConverter> frame_converter,
    std::vector<Fourcc> renderable_fourccs,
    std::unique_ptr<MediaLog> media_log,
    mojo::PendingRemote<stable::mojom::StableVideoDecoder> oop_video_decoder,
    bool in_video_decoder_process) {
  DCHECK(client_task_runner);
  DCHECK(frame_pool);
  DCHECK(!renderable_fourccs.empty());

  CreateDecoderFunctionCB create_decoder_function_cb;
  bool uses_oop_video_decoder = false;
  if (oop_video_decoder) {
    create_decoder_function_cb =
        base::BindOnce(&OOPVideoDecoder::Create, std::move(oop_video_decoder));
    uses_oop_video_decoder = true;
  } else {
#if BUILDFLAG(USE_VAAPI)
    create_decoder_function_cb = base::BindOnce(&VaapiVideoDecoder::Create);
#elif BUILDFLAG(USE_V4L2_CODEC)
    if (base::FeatureList::IsEnabled(kV4L2FlatStatefulVideoDecoder) &&
        IsV4L2DecoderStateful()) {
      create_decoder_function_cb =
          base::BindOnce(&V4L2StatefulVideoDecoder::Create);
    } else {
      create_decoder_function_cb = base::BindOnce(&V4L2VideoDecoder::Create);
    }
#else
    return nullptr;
#endif
  }

  auto* pipeline = new VideoDecoderPipeline(
      workarounds, std::move(client_task_runner), std::move(frame_pool),
      std::move(frame_converter), std::move(renderable_fourccs),
      std::move(media_log), std::move(create_decoder_function_cb),
      uses_oop_video_decoder, in_video_decoder_process);
  return std::make_unique<AsyncDestroyVideoDecoder<VideoDecoderPipeline>>(
      base::WrapUnique(pipeline));
}

// static
std::unique_ptr<VideoDecoder> VideoDecoderPipeline::CreateForVDAAdapterForARC(
    const gpu::GpuDriverBugWorkarounds& workarounds,
    scoped_refptr<base::SequencedTaskRunner> client_task_runner,
    std::unique_ptr<DmabufVideoFramePool> frame_pool,
    std::vector<Fourcc> renderable_fourccs) {
  DCHECK(client_task_runner);
  DCHECK(frame_pool);
  DCHECK(!renderable_fourccs.empty());

  CreateDecoderFunctionCB create_decoder_function_cb;
#if BUILDFLAG(USE_VAAPI)
  create_decoder_function_cb = base::BindOnce(&VaapiVideoDecoder::Create);
#elif BUILDFLAG(USE_V4L2_CODEC)
  create_decoder_function_cb = base::BindOnce(&V4L2VideoDecoder::Create);
#else
  return nullptr;
#endif

  auto* pipeline = new VideoDecoderPipeline(
      workarounds, std::move(client_task_runner), std::move(frame_pool),
      /*frame_converter=*/nullptr, std::move(renderable_fourccs),
      std::make_unique<NullMediaLog>(), std::move(create_decoder_function_cb),
      /*uses_oop_video_decoder=*/false,
      // TODO(b/195769334): Set this properly once OOP-VD is enabled for ARC.
      /*in_video_decoder_process=*/false);
  return std::make_unique<AsyncDestroyVideoDecoder<VideoDecoderPipeline>>(
      base::WrapUnique(pipeline));
}

// static
std::unique_ptr<VideoDecoder> VideoDecoderPipeline::CreateForTesting(
    scoped_refptr<base::SequencedTaskRunner> client_task_runner,
    std::unique_ptr<MediaLog> media_log,
    bool ignore_resolution_changes_to_smaller_for_testing) {
  CreateDecoderFunctionCB create_decoder_function_cb;
#if BUILDFLAG(USE_VAAPI)
  create_decoder_function_cb = base::BindOnce(&VaapiVideoDecoder::Create);
#elif BUILDFLAG(USE_V4L2_CODEC)
  if (base::FeatureList::IsEnabled(kV4L2FlatStatefulVideoDecoder) &&
      IsV4L2DecoderStateful()) {
    create_decoder_function_cb =
        base::BindOnce(&V4L2StatefulVideoDecoder::Create);
  } else {
    create_decoder_function_cb = base::BindOnce(&V4L2VideoDecoder::Create);
  }
#endif

  auto* pipeline = new VideoDecoderPipeline(
      gpu::GpuDriverBugWorkarounds(), std::move(client_task_runner),
      std::make_unique<PlatformVideoFramePool>(),
      /*frame_converter=*/nullptr,
      VideoDecoderPipeline::DefaultPreferredRenderableFourccs(),
      std::move(media_log), std::move(create_decoder_function_cb),
      /*uses_oop_video_decoder=*/false,
      /*in_video_decoder_process=*/true);

  if (ignore_resolution_changes_to_smaller_for_testing)
    pipeline->ignore_resolution_changes_to_smaller_for_testing_ = true;

  return std::make_unique<AsyncDestroyVideoDecoder<VideoDecoderPipeline>>(
      base::WrapUnique(pipeline));
}

// static
std::vector<Fourcc> VideoDecoderPipeline::DefaultPreferredRenderableFourccs() {
  // Preferred output formats in order of preference.
  // TODO(mcasas): query the platform for its preferred formats and modifiers.
  return {
      Fourcc(Fourcc::NV12),
      Fourcc(Fourcc::P010),
      // Only used for Hana (MT8173). Remove when that device reaches EOL
      Fourcc(Fourcc::YV12),
  };
}

// static
void VideoDecoderPipeline::NotifySupportKnown(
    mojo::PendingRemote<stable::mojom::StableVideoDecoder> oop_video_decoder,
    base::OnceCallback<
        void(mojo::PendingRemote<stable::mojom::StableVideoDecoder>)> cb) {
  if (oop_video_decoder) {
    OOPVideoDecoder::NotifySupportKnown(std::move(oop_video_decoder),
                                        std::move(cb));
    return;
  }
  std::move(cb).Run(std::move(oop_video_decoder));
}

// static
std::optional<SupportedVideoDecoderConfigs>
VideoDecoderPipeline::GetSupportedConfigs(
    VideoDecoderType decoder_type,
    const gpu::GpuDriverBugWorkarounds& workarounds) {
  std::optional<SupportedVideoDecoderConfigs> configs;
  switch (decoder_type) {
    case VideoDecoderType::kOutOfProcess:
      configs = OOPVideoDecoder::GetSupportedConfigs();
      break;
#if BUILDFLAG(USE_VAAPI)
    case VideoDecoderType::kVaapi:
      configs = VaapiVideoDecoder::GetSupportedConfigs();
      break;
#elif BUILDFLAG(USE_V4L2_CODEC)
    case VideoDecoderType::kV4L2:
      configs = GetSupportedV4L2DecoderConfigs();
      break;
#endif
    default:
      configs = std::nullopt;
  }

  if (!configs)
    return std::nullopt;

  if (workarounds.disable_accelerated_vp8_decode) {
    std::erase_if(configs.value(), [](const auto& config) {
      return config.profile_min >= VP8PROFILE_MIN &&
             config.profile_max <= VP8PROFILE_MAX;
    });
  }

  if (workarounds.disable_accelerated_vp9_decode) {
    std::erase_if(configs.value(), [](const auto& config) {
      return config.profile_min >= VP9PROFILE_PROFILE0 &&
             config.profile_max <= VP9PROFILE_PROFILE0;
    });
  }

  if (workarounds.disable_accelerated_vp9_profile2_decode) {
    std::erase_if(configs.value(), [](const auto& config) {
      return config.profile_min >= VP9PROFILE_PROFILE2 &&
             config.profile_max <= VP9PROFILE_PROFILE2;
    });
  }

  if (workarounds.disable_accelerated_h264_decode) {
    std::erase_if(configs.value(), [](const auto& config) {
      return config.profile_min >= H264PROFILE_MIN &&
             config.profile_max <= H264PROFILE_MAX;
    });
  }

  if (workarounds.disable_accelerated_hevc_decode) {
    std::erase_if(configs.value(), [](const auto& config) {
      return config.profile_min >= HEVCPROFILE_MIN &&
             config.profile_max <= HEVCPROFILE_MAX;
    });
  }

  return configs;
}

VideoDecoderPipeline::VideoDecoderPipeline(
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
    scoped_refptr<base::SequencedTaskRunner> client_task_runner,
    std::unique_ptr<DmabufVideoFramePool> frame_pool,
    std::unique_ptr<FrameResourceConverter> frame_converter,
    std::vector<Fourcc> renderable_fourccs,
    std::unique_ptr<MediaLog> media_log,
    CreateDecoderFunctionCB create_decoder_function_cb,
    bool uses_oop_video_decoder,
    bool in_video_decoder_process)
    : gpu_workarounds_(gpu_workarounds),
      client_task_runner_(std::move(client_task_runner)),
      decoder_task_runner_(
          uses_oop_video_decoder
              ? client_task_runner_
              : GetDecoderTaskRunner(in_video_decoder_process)),
      main_frame_pool_(std::move(frame_pool)),
      frame_converter_(frame_converter ? std::move(frame_converter)
                                       : DefaultFrameConverter::Create()),
      renderable_fourccs_(std::move(renderable_fourccs)),
      media_log_(std::move(media_log)),
      create_decoder_function_cb_(std::move(create_decoder_function_cb)),
      oop_decoder_can_read_without_stalling_(false),
      uses_oop_video_decoder_(uses_oop_video_decoder) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  DETACH_FROM_SEQUENCE(decoder_sequence_checker_);
  DCHECK(main_frame_pool_);
  DCHECK(client_task_runner_);
  DVLOGF(2);

  decoder_weak_this_ = decoder_weak_this_factory_.GetWeakPtr();

  main_frame_pool_->set_parent_task_runner(decoder_task_runner_);
  frame_converter_->Initialize(
      decoder_task_runner_,
      base::BindRepeating(&VideoDecoderPipeline::OnFrameConverted,
                          decoder_weak_this_));
}

VideoDecoderPipeline::~VideoDecoderPipeline() {
  // We have to destroy |main_frame_pool_| and |frame_converter_| on
  // |decoder_task_runner_|, so the destructor must be called on
  // |decoder_task_runner_|.
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(3);

  decoder_weak_this_factory_.InvalidateWeakPtrs();

  // Destroy |frame_converter_| before |main_frame_pool_| and |decoder| because
  // the former may have a raw pointer to the latter (in the unwrap-frame
  // callback).
  //
  // TODO(andrescj): consider making the unwrap-frame callback work with WeakPtr
  // instead.
  frame_converter_.reset();
  main_frame_pool_.reset();
#if BUILDFLAG(IS_CHROMEOS)
  // We must release |buffer_transcryptor_| before the decoder because it holds
  // a raw pointer to |decoder_|.
  buffer_transcryptor_.reset();
#endif  // BUILDFLAG(IS_CHROMEOS)
  decoder_.reset();
}

// static
void VideoDecoderPipeline::DestroyAsync(
    std::unique_ptr<VideoDecoderPipeline> pipeline) {
  DVLOGF(2);
  DCHECK(pipeline);
  DCHECK_CALLED_ON_VALID_SEQUENCE(pipeline->client_sequence_checker_);

  auto* decoder_task_runner = pipeline->decoder_task_runner_.get();
  decoder_task_runner->DeleteSoon(FROM_HERE, std::move(pipeline));
}

VideoDecoderType VideoDecoderPipeline::GetDecoderType() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  // TODO(mcasas): query |decoder_| instead. This is difficult because it can
  // only be accessed on |decoder_sequence_checker_|: this method is supposed
  // to be synchronous.

  if (uses_oop_video_decoder_) {
    return VideoDecoderType::kOutOfProcess;
  }

#if BUILDFLAG(USE_VAAPI)
  return VideoDecoderType::kVaapi;
#elif BUILDFLAG(USE_V4L2_CODEC)
  return VideoDecoderType::kV4L2;
#else
  return VideoDecoderType::kUnknown;
#endif
}

bool VideoDecoderPipeline::IsPlatformDecoder() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  return true;
}

int VideoDecoderPipeline::GetMaxDecodeRequests() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  return decoder_max_decode_requests_;
}

bool VideoDecoderPipeline::FramesHoldExternalResources() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  return true;
}

bool VideoDecoderPipeline::NeedsBitstreamConversion() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  // TODO(mcasas): also query |decoder_|.
  return needs_bitstream_conversion_;
}

bool VideoDecoderPipeline::CanReadWithoutStalling() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  if (uses_oop_video_decoder_) {
    return oop_decoder_can_read_without_stalling_.load(
        std::memory_order_seq_cst);
  }

  // TODO(mcasas): also query |decoder_|.
  return main_frame_pool_ && !main_frame_pool_->IsExhausted();
}

size_t VideoDecoderPipeline::GetDecoderMaxOutputFramePoolSize() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);

  return decoder_ ? decoder_->GetMaxOutputFramePoolSize()
                  : std::numeric_limits<size_t>::max();
}

void VideoDecoderPipeline::Initialize(const VideoDecoderConfig& config,
                                      bool low_delay,
                                      CdmContext* cdm_context,
                                      InitCB init_cb,
                                      const OutputCB& output_cb,
                                      const WaitingCB& waiting_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  VLOGF(2) << "config: " << config.AsHumanReadableString();

  if (!config.IsValidConfig()) {
    VLOGF(1) << "config is not valid";
    std::move(init_cb).Run(DecoderStatus::Codes::kUnsupportedConfig);
    return;
  }
#if BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)
  if (config.is_encrypted() && !cdm_context) {
    VLOGF(1) << "Encrypted streams require a CdmContext";
    std::move(init_cb).Run(DecoderStatus::Codes::kUnsupportedConfig);
    return;
  }
#else   // BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)
  if (config.is_encrypted() && !allow_encrypted_content_for_testing_) {
    VLOGF(1) << "Encrypted streams are not supported for this VD";
    std::move(init_cb).Run(DecoderStatus::Codes::kUnsupportedEncryptionMode);
    return;
  }
#endif  // !BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)

  // Make sure that the configuration requested is supported by the driver,
  // which must provide such information.
  const auto supported_configs =
      supported_configs_for_testing_.empty()
          ? VideoDecoderPipeline::GetSupportedConfigs(GetDecoderType(),
                                                      gpu_workarounds_)
          : supported_configs_for_testing_;
  if (!supported_configs.has_value()) {
    std::move(init_cb).Run(DecoderStatus::Codes::kUnsupportedConfig);
    return;
  }
  if (!IsVideoDecoderConfigSupported(supported_configs.value(), config)) {
    VLOGF(1) << "Video configuration is not supported: "
             << config.AsHumanReadableString();
    MEDIA_LOG(INFO, media_log_) << "Video configuration is not supported: "
                                << config.AsHumanReadableString();
    std::move(init_cb).Run(DecoderStatus::Codes::kUnsupportedConfig);
    return;
  }
  constexpr auto kVideoCodecProfileCount = media::VIDEO_CODEC_PROFILE_MAX + 1;
  base::UmaHistogramEnumeration(
      "Media.PlatformVideoDecoding.VideoCodecProfile", config.profile(),
      static_cast<VideoCodecProfile>(kVideoCodecProfileCount));

  needs_bitstream_conversion_ = (config.codec() == VideoCodec::kH264) ||
                                (config.codec() == VideoCodec::kHEVC);

  decoder_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoDecoderPipeline::InitializeTask, decoder_weak_this_,
                     config, low_delay, cdm_context, std::move(init_cb),
                     std::move(output_cb), std::move(waiting_cb)));
}

void VideoDecoderPipeline::InitializeTask(const VideoDecoderConfig& config,
                                          bool low_delay,
                                          CdmContext* cdm_context,
                                          InitCB init_cb,
                                          const OutputCB& output_cb,
                                          const WaitingCB& waiting_cb) {
  DVLOGF(3);
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);

  client_output_cb_ = std::move(output_cb);
  waiting_cb_ = std::move(waiting_cb);

  // |decoder_| may be Initialize()d multiple times (e.g. on |config| changes)
  // but can only be created once.
  if (!decoder_ && !create_decoder_function_cb_.is_null()) {
    // Note: because we std::move(create_decoder_function_cb_), we only reach
    // this code once. Therefore, we don't need to worry about this assignment
    // potentially destroying an existing |decoder_| which means we don't have
    // to call |frame_converter_|->set_get_original_frame_cb() here.
    decoder_ =
        std::move(create_decoder_function_cb_)
            .Run(media_log_->Clone(), decoder_task_runner_, decoder_weak_this_);
  }
  // Note: |decoder_| might fail to be created, e.g. on V4L2 platforms.
  if (!decoder_) {
    OnError("|decoder_| creation failed.");
    client_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(init_cb),
                       DecoderStatus::Codes::kFailedToCreateDecoder));
    return;
  }

  if (frame_converter_->UsesGetOriginalFrameCB()) {
    FrameResourceConverter::GetOriginalFrameCB get_original_frame_cb;

    if (uses_oop_video_decoder_) {
      // Note: base::Unretained() is safe because either a) |decoder_| outlives
      // the |frame_converter_| or b) we call
      // |frame_converter_|->set_get_original_frame_cb() with a null
      // GetOriginalFrameCB before destroying |decoder_|.
      get_original_frame_cb = base::BindRepeating(
          &OOPVideoDecoder::GetOriginalFrame,
          base::Unretained(static_cast<OOPVideoDecoder*>(decoder_.get())));
    } else {
      CHECK(main_frame_pool_);
      PlatformVideoFramePool* platform_video_frame_pool =
          main_frame_pool_->AsPlatformVideoFramePool();
      // The only |frame_converter_| that needs the GetOriginalFrameCB callback
      // is the MailboxVideoFrameConverter. When it is used, the
      // |main_frame_pool_| should always be a PlatformVideoFramePool.
      CHECK(platform_video_frame_pool);

      // Note: base::Unretained() is safe because either a) the
      // |main_frame_pool_| outlives |frame_converter_| or b) we call
      // |frame_converter_|->set_get_original_frame_cb() with a null
      // GetOriginalFrameCB before destroying |main_frame_pool_|.
      get_original_frame_cb =
          base::BindRepeating(&PlatformVideoFramePool::GetOriginalFrame,
                              base::Unretained(platform_video_frame_pool));
    }

    frame_converter_->set_get_original_frame_cb(
        std::move(get_original_frame_cb));
  }

  estimated_num_buffers_for_renderer_ =
      EstimateRequiredRendererPipelineBuffers(low_delay, config.is_encrypted());

#if BUILDFLAG(USE_V4L2_CODEC)
  decryption_needs_vp9_superframe_splitting_ =
      config.codec() == VideoCodec::kVP9;
#endif

#if BUILDFLAG(USE_VAAPI)
  if (ignore_resolution_changes_to_smaller_for_testing_) {
    static_cast<VaapiVideoDecoder*>(decoder_.get())
        ->set_ignore_resolution_changes_to_smaller_vp9_for_testing(  // IN-TEST
            true);
  }
#endif

  decoder_->Initialize(
      config, /* low_delay=*/false, cdm_context,
      base::BindOnce(&VideoDecoderPipeline::OnInitializeDone,
                     decoder_weak_this_, std::move(init_cb), cdm_context),
      base::BindRepeating(&VideoDecoderPipeline::OnFrameDecoded,
                          decoder_weak_this_),
      base::BindRepeating(&VideoDecoderPipeline::OnDecoderWaiting,
                          decoder_weak_this_));
}

void VideoDecoderPipeline::OnInitializeDone(InitCB init_cb,
                                            CdmContext* cdm_context,
                                            DecoderStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(4) << "Initialization status = " << static_cast<int>(status.code());

  if (!status.is_ok()) {
    MEDIA_LOG(ERROR, media_log_)
        << "VideoDecoderPipeline |decoder_| Initialize() failed, status: "
        << static_cast<int>(status.code());
    frame_converter_->set_get_original_frame_cb(base::NullCallback());
#if BUILDFLAG(IS_CHROMEOS)
    // We always need to destroy |buffer_transcryptor_| if it exists before
    // |decoder_|.
    buffer_transcryptor_.reset();
#endif  // BUILDFLAG(IS_CHROMEOS)
    decoder_.reset();
  } else {
    MEDIA_LOG(INFO, media_log_)
        << "VideoDecoderPipeline |decoder_| Initialize() successful";
  }

#if BUILDFLAG(IS_CHROMEOS)
  if (decoder_ && decoder_->NeedsTranscryption()) {
    if (!cdm_context) {
      VLOGF(1) << "CdmContext required for transcryption";
      frame_converter_->set_get_original_frame_cb(base::NullCallback());
      // We always need to destroy |buffer_transcryptor_| if it exists before
      // |decoder_|.
      buffer_transcryptor_.reset();
      decoder_.reset();
      status = DecoderStatus::Codes::kUnsupportedEncryptionMode;
    } else {
      // We need to enable transcryption for protected content.
      buffer_transcryptor_ = std::make_unique<DecoderBufferTranscryptor>(
          cdm_context, *decoder_, decryption_needs_vp9_superframe_splitting_,
          base::BindRepeating(&VideoDecoderPipeline::OnBufferTranscrypted,
                              decoder_weak_this_),
          base::BindRepeating(&VideoDecoderPipeline::OnDecoderWaiting,
                              decoder_weak_this_));
    }
  } else {
    // In case this was created on a prior initialization but no longer needed.
    buffer_transcryptor_.reset();
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  client_task_runner_->PostTask(FROM_HERE,
                                base::BindOnce(std::move(init_cb), status));
}

void VideoDecoderPipeline::Reset(base::OnceClosure reset_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  DVLOGF(3);

  decoder_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VideoDecoderPipeline::ResetTask,
                                decoder_weak_this_, std::move(reset_cb)));
}

void VideoDecoderPipeline::ResetTask(base::OnceClosure reset_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(3);

#if BUILDFLAG(IS_CHROMEOS)
  drop_transcrypted_buffers_ = true;
#endif  // BUILDFLAG(IS_CHROMEOS)
  need_apply_new_resolution = false;
  decoder_->Reset(base::BindOnce(&VideoDecoderPipeline::OnResetDone,
                                 decoder_weak_this_, std::move(reset_cb)));
}

void VideoDecoderPipeline::OnResetDone(base::OnceClosure reset_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(3);

  if (image_processor_)
    image_processor_->Reset();
  frame_converter_->AbortPendingFrames();

#if BUILDFLAG(IS_CHROMEOS)
  if (buffer_transcryptor_)
    buffer_transcryptor_->Reset(DecoderStatus::Codes::kAborted);
#endif  // BUILDFLAG(IS_CHROMEOS)

  CallFlushCbIfNeeded(/*override_status=*/DecoderStatus::Codes::kAborted);

  if (need_frame_pool_rebuild_) {
    need_frame_pool_rebuild_ = false;
    if (main_frame_pool_)
      main_frame_pool_->ReleaseAllFrames();
    if (auxiliary_frame_pool_)
      auxiliary_frame_pool_->ReleaseAllFrames();
  }

#if BUILDFLAG(IS_CHROMEOS)
  drop_transcrypted_buffers_ = false;
#endif  // BUILDFLAG(IS_CHROMEOS)

  client_task_runner_->PostTask(FROM_HERE, std::move(reset_cb));
}

void VideoDecoderPipeline::Decode(scoped_refptr<DecoderBuffer> buffer,
                                  DecodeCB decode_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  CHECK(buffer);
  DVLOGF(4);
  TRACE_EVENT1(
      "media,gpu", "VideoDecoderPipeline::Decode", "timestamp",
      (buffer->end_of_stream() ? 0 : buffer->timestamp().InMicroseconds()));
  decoder_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoDecoderPipeline::DecodeTask, decoder_weak_this_,
                     std::move(buffer), std::move(decode_cb)));
}

void VideoDecoderPipeline::DecodeTask(scoped_refptr<DecoderBuffer> buffer,
                                      DecodeCB decode_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DCHECK(decoder_);
  CHECK(buffer);
  DVLOGF(4);
  TRACE_EVENT1(
      "media,gpu", "VideoDecoderPipeline::DecodeTask", "timestamp",
      (buffer->end_of_stream() ? 0 : buffer->timestamp().InMicroseconds()));
  if (has_error_) {
    client_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(decode_cb), DecoderStatus::Codes::kFailed));
    return;
  }

  const bool is_flush = buffer->end_of_stream();
#if BUILDFLAG(IS_CHROMEOS)
  if (buffer_transcryptor_) {
    buffer_transcryptor_->EnqueueBuffer(
        std::move(buffer),
        base::BindOnce(&VideoDecoderPipeline::OnDecodeDone, decoder_weak_this_,
                       is_flush, std::move(decode_cb)));
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  decoder_->Decode(
      std::move(buffer),
      base::BindOnce(&VideoDecoderPipeline::OnDecodeDone, decoder_weak_this_,
                     is_flush, std::move(decode_cb)));
}

void VideoDecoderPipeline::OnDecodeDone(bool is_flush,
                                        DecodeCB decode_cb,
                                        DecoderStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(4) << "is_flush: " << is_flush
            << ", status: " << static_cast<int>(status.code());

  if (has_error_)
    status = DecoderStatus::Codes::kFailed;

  if (is_flush) {
    client_flush_cb_state_.emplace(
        /*flush_cb=*/std::move(decode_cb), /*decoder_decode_status=*/status);
    CallFlushCbIfNeeded(/*override_status=*/std::nullopt);
    return;
  }

  client_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(decode_cb), std::move(status)));
}

void VideoDecoderPipeline::OnFrameDecoded(scoped_refptr<FrameResource> frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(4);
  TRACE_EVENT1("media,gpu", "VideoDecoderPipeline::OnFrameDecoded", "timestamp",
               (frame ? frame->timestamp().InMicroseconds() : 0));

#if BUILDFLAG(IS_CHROMEOS)
  if (buffer_transcryptor_) {
    buffer_transcryptor_->SecureBuffersMayBeAvailable();
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  if (uses_oop_video_decoder_) {
    oop_decoder_can_read_without_stalling_.store(
        decoder_->CanReadWithoutStalling(), std::memory_order_seq_cst);
  }

  if (image_processor_) {
    image_processor_->Process(
        std::move(frame),
        base::BindOnce(&VideoDecoderPipeline::OnFrameProcessed,
                       decoder_weak_this_));
    return;
  }

  frame_converter_->ConvertFrame(std::move(frame));
}

void VideoDecoderPipeline::OnFrameProcessed(
    scoped_refptr<FrameResource> frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(4);
  TRACE_EVENT1("media,gpu", "VideoDecoderPipeline::OnFrameProcessed",
               "timestamp", (frame ? frame->timestamp().InMicroseconds() : 0));
  frame_converter_->ConvertFrame(std::move(frame));
}

void VideoDecoderPipeline::OnFrameConverted(
    scoped_refptr<VideoFrame> video_frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(4);
  TRACE_EVENT1("media,gpu", "VideoDecoderPipeline::OnFrameConverted",
               "timestamp",
               (video_frame ? video_frame->timestamp().InMicroseconds() : 0));
  if (!video_frame) {
    return OnError("Frame converter returns null frame.");
  }
  if (has_error_) {
    DVLOGF(2) << "Skip returning frames after error occurs.";
    return;
  }

  // Flag that the video frame was decoded in a power efficient way.
  video_frame->metadata().power_efficient = true;

  // MojoVideoDecoderService expects the |output_cb_| to be called on the client
  // task runner, even though media::VideoDecoder states frames should be output
  // without any thread jumping.
  // Note that all the decode/flush/output/reset callbacks are executed on
  // |client_task_runner_|.
  client_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(client_output_cb_, std::move(video_frame)));

  // After outputting a frame, flush might be completed.
  CallFlushCbIfNeeded(/*override_status=*/std::nullopt);
  CallApplyResolutionChangeIfNeeded();
}

void VideoDecoderPipeline::OnDecoderWaiting(WaitingReason reason) {
  DVLOGF(3);
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  if (reason == media::WaitingReason::kDecoderStateLost)
    need_frame_pool_rebuild_ = true;

  client_task_runner_->PostTask(FROM_HERE, base::BindOnce(waiting_cb_, reason));
}

bool VideoDecoderPipeline::HasPendingFrames() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  const bool frame_converter_has_pending_frames_ =
      frame_converter_->HasPendingFrames();
  const bool image_processor_has_pending_frames_ =
      image_processor_ && image_processor_->HasPendingFrames();

  DVLOGF(3) << "|frame_converter_|: "
            << (frame_converter_has_pending_frames_ ? "yes" : "no")
            << ", |image_processor_|: "
            << (image_processor_has_pending_frames_ ? "yes" : "no");
  return frame_converter_has_pending_frames_ ||
         image_processor_has_pending_frames_;
}

void VideoDecoderPipeline::OnError(const std::string& msg) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  VLOGF(1) << msg;
  MEDIA_LOG(ERROR, media_log_) << "VideoDecoderPipeline " << msg;

  has_error_ = true;

  if (image_processor_)
    image_processor_->Reset();
  frame_converter_->AbortPendingFrames();

#if BUILDFLAG(IS_CHROMEOS)
  if (buffer_transcryptor_)
    buffer_transcryptor_->Reset(DecoderStatus::Codes::kFailed);
#endif  // BUILDFLAG(IS_CHROMEOS)

  CallFlushCbIfNeeded(/*override_status=*/DecoderStatus::Codes::kFailed);
}

void VideoDecoderPipeline::CallFlushCbIfNeeded(
    std::optional<DecoderStatus> override_status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);

  if (!client_flush_cb_state_) {
    return;
  }

  // All the call sites where |override_status| is non-null should guarantee
  // that there are no pending frames. If there were pending frames, then we
  // would drop |override_status|, and that seems like undesired behavior.
  const bool has_pending_frames = HasPendingFrames();
  DCHECK(!override_status || !has_pending_frames);

  if (has_pending_frames) {
    // Flush is not completed yet.
    return;
  }

  DecodeCB flush_cb = std::move(client_flush_cb_state_->flush_cb);
  const DecoderStatus status =
      override_status.value_or(client_flush_cb_state_->decoder_decode_status);
  DVLOGF(3) << "status: " << static_cast<int>(status.code());
  client_flush_cb_state_.reset();
  client_task_runner_->PostTask(FROM_HERE,
                                base::BindOnce(std::move(flush_cb), status));
}

void VideoDecoderPipeline::PrepareChangeResolution() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(3);
  DCHECK(!need_apply_new_resolution);

  need_apply_new_resolution = true;
  CallApplyResolutionChangeIfNeeded();
}

void VideoDecoderPipeline::CallApplyResolutionChangeIfNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(4);

  if (need_apply_new_resolution && !HasPendingFrames()) {
    need_apply_new_resolution = false;
    decoder_->ApplyResolutionChange();
  }
}

DmabufVideoFramePool* VideoDecoderPipeline::GetVideoFramePool() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);

  // TODO(andrescj): consider returning a WeakPtr instead. That way, if callers
  // store the returned pointer, they know that they should check it's valid
  // because the video frame pool can change across resolution changes if we go
  // from using an image processor to not using one (or viceversa).
  if (image_processor_)
    return auxiliary_frame_pool_.get();
  return main_frame_pool_.get();
}

void VideoDecoderPipeline::NotifyEstimatedMaxDecodeRequests(int num) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DCHECK(num);
  decoder_max_decode_requests_ = num;
}

CroStatus::Or<PixelLayoutCandidate>
VideoDecoderPipeline::PickDecoderOutputFormat(
    const std::vector<PixelLayoutCandidate>& candidates,
    const gfx::Rect& decoder_visible_rect,
    const gfx::Size& decoder_natural_size,
    std::optional<gfx::Size> output_size,
    size_t num_codec_reference_frames,
    bool use_protected,
    bool need_aux_frame_pool,
    std::optional<DmabufVideoFramePool::CreateFrameCB> allocator) {
  DVLOGF(3);
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  // Verify |num_codec_reference_frames| has a reasonable value. Anecdotally 16
  // is the largest amount of reference frames seen, on an ITU-T H.264 test
  // vector (CAPCM*1_Sand_E.h264).
  CHECK_LE(num_codec_reference_frames, 32u);

  if (candidates.empty())
    return CroStatus::Codes::kNoDecoderOutputFormatCandidates;

  auxiliary_frame_pool_.reset();
  image_processor_.reset();

  // As long as we're not scaling, check if any of the |candidates| formats is
  // directly renderable. If so, and (VA-API-only) the modifier of buffers
  // provided by the frame pool matches the one supported by the |decoder_|, we
  // don't need an image processor.
  std::optional<PixelLayoutCandidate> viable_candidate;
  if (!output_size || *output_size == decoder_visible_rect.size()) {
    for (const auto& fourcc : renderable_fourccs_) {
      for (const auto& candidate : candidates) {
        if (candidate.fourcc == fourcc) {
          viable_candidate = candidate;
          break;
        }
      }
      if (viable_candidate)
        break;
    }
  }

  // TODO(jkardatzke): Remove this when we have protected content rendering on
  // ARM working. This is temporary so that video will actually decode on HWDRM
  // ARM devices during development (even though it won't be visible).
#if BUILDFLAG(USE_V4L2_CODEC) && BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)
  if (use_protected) {
    for (const auto& candidate : candidates) {
      if (candidate.fourcc == Fourcc(Fourcc::MM21) ||
          candidate.fourcc == Fourcc(Fourcc::MT2T)) {
        LOG(WARNING) << "Forcing MM21/MT2T format for V4L2 protected content";
        viable_candidate = candidate;
      }
    }
  }
#endif

#if BUILDFLAG(IS_LINUX) && BUILDFLAG(USE_VAAPI)
  // Linux should always use a custom allocator (to allocate buffers using
  // libva) and a PlatformVideoFramePool.
  CHECK(allocator.has_value());
  CHECK(main_frame_pool_->AsPlatformVideoFramePool());
  // The custom allocator creates frames backed by NativePixmap, which uses a
  // VideoFrame::StorageType of VideoFrame::STORAGE_DMABUFS.
  main_frame_pool_->AsPlatformVideoFramePool()->SetCustomFrameAllocator(
      *allocator, VideoFrame::STORAGE_DMABUFS);
#elif BUILDFLAG(IS_LINUX) && BUILDFLAG(USE_V4L2_CODEC)
  // Linux w/ V4L2 should not use a custom allocator
  // Only tested with video_decode_accelerator_tests
  // TODO(wenst@) Test with full Chromium Browser
  CHECK(!allocator.has_value());
  if (viable_candidate) {
    // Instead, let V4L2 allocate the buffers if it can decode directly
    // to the preferred formats. There's no need to allocate frames.
    // This is not compatible with VdVideoDecodeAccelerator, which
    // expects GPU buffers in VdVideoDecodeAccelerator::GetPicture()
    frame_converter_->set_get_original_frame_cb(base::NullCallback());
    main_frame_pool_.reset();
    return *viable_candidate;
  }
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  // Lacros should always use a PlatformVideoFramePool outside of tests (because
  // it doesn't need to handle ARC++/ARCVM requests) with no custom allocator
  // (because buffers are allocated with minigbm).
  CHECK(!allocator.has_value());
  CHECK(main_frame_pool_->AsPlatformVideoFramePool() ||
        main_frame_pool_->IsFakeVideoFramePool());
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  // Ash Chrome can use any type of frame pool (because it may get requests from
  // ARC++/ARCVM) but never a custom allocator.
  CHECK(!allocator.has_value());
#else
#error "Unsupported platform"
#endif

  if (viable_candidate) {
    // If maximum decoder frame pool size is less than the number of codec
    // reference frames (plus one frame for the frame being decoded), then the
    // decode will stall. Instead, this returns an error.
    if ((num_codec_reference_frames + 1) > GetDecoderMaxOutputFramePoolSize()) {
      return CroStatus::Codes::kInsufficientFramePoolSize;
    }

    // |main_frame_pool_| needs to allocate enough buffers for both the codec
    // reference needs and the Renderer pipeline.
    // |num_codec_reference_frames| is augmented by 1 to account for the frame
    // being decoded.
    const size_t num_pictures = std::min(
        GetDecoderMaxOutputFramePoolSize(),
        num_codec_reference_frames + 1 + estimated_num_buffers_for_renderer_);
    VLOGF(1) << "Initializing frame pool with up to " << num_pictures
             << " VideoFrames. No ImageProcessor needed.";

#if BUILDFLAG(USE_V4L2_CODEC)
    if (use_protected) {
      // Check to make sure we aren't going to blow our memory budget for V4L2
      // secure playback. We have 210MB reserved for the frame pool we allocate
      // here.
      constexpr size_t kMaxV4L2ProtectedMemory = 210 * 1024 * 1024;
      // Resolution
      size_t mem_required =
          viable_candidate->size.width() * viable_candidate->size.height();
      // YUV 4:2:0
      mem_required = mem_required * 3 / 2;
      if (viable_candidate->fourcc == Fourcc(Fourcc::MT2T)) {
        // 10-bit
        mem_required = mem_required * 5 / 4;
      }
      // For each picture
      mem_required *= num_pictures;
      VLOGF(1) << "V4L2 secure memory requirement is "
               << (mem_required / (1024 * 1024)) << "MB";
      if (mem_required > kMaxV4L2ProtectedMemory) {
        LOG(ERROR) << "Exceeded max memory for secure V4L2: "
                   << (mem_required / (1024 * 1024)) << "MB of "
                   << (kMaxV4L2ProtectedMemory / (1024 * 1024)) << "MB";
        return CroStatus::Codes::kUnableToAllocateSecureBuffer;
      }
    }
#endif  // BUILDFLAG(USE_V4L2_CODEC)

    CroStatus::Or<GpuBufferLayout> status_or_layout =
        main_frame_pool_->Initialize(viable_candidate->fourcc,
                                     viable_candidate->size,
                                     decoder_visible_rect, decoder_natural_size,
                                     num_pictures, use_protected);
    if (!status_or_layout.has_value())
      return std::move(status_or_layout).error();

    // TODO(mcasas): Consider changing the code here to update
    // viable_candidate->modifier to be |status_or_layout|'s modifier(), so
    // that callers of this method don't need to inspect GetGpuBufferLayout()
    // of this class' GetVideoFramePool().

#if BUILDFLAG(USE_VAAPI) && BUILDFLAG(IS_CHROMEOS_ASH)
    // Linux and Lacros do not check the modifiers,
    // since they do not set any.
    const GpuBufferLayout layout(std::move(status_or_layout).value());
    if (layout.modifier() == viable_candidate->modifier) {
      return *viable_candidate;
    } else if (layout.modifier() != DRM_FORMAT_MOD_LINEAR) {
      // In theory, we could accept any |layout|.modifier(). However, the only
      // known use case for a modifier different than the one native to the
      // |decoder_| is when Android wishes to get linear decoded data. Thus, to
      // reduce the number of of moving parts that can fail, we restrict the
      // modifiers of pool buffers to be either the hardware decoder's native
      // modifier or DRM_FORMAT_MOD_LINEAR.
      DVLOGF(2) << "Unsupported modifier, " << std::hex
                << viable_candidate->modifier << ", passed in";
      return CroStatus::Codes::kFailedToCreateImageProcessor;
    }
#else
    return *viable_candidate;
#endif  // BUILDFLAG(USE_VAAPI) && BUILDFLAG(IS_CHROMEOS_ASH)
  }

  // We haven't found a |viable_candidate|, and need to instantiate an
  // ImageProcessor; this might need to allocate buffers internally, but only
  // to fill the Renderer pipeline.
  // TODO(b/267691989): The number of buffers for the image processor may need
  // need to be limited with a mechanism similar to
  // VideoDecoderMixin::GetMaxOutputFramePoolSize() depending on the backend.
  // Consider exposing the max frame pool size to the ImageProcessor.
  std::unique_ptr<ImageProcessor> image_processor;
  if (create_image_processor_cb_for_testing_) {
    image_processor = create_image_processor_cb_for_testing_.Run(
        candidates,
        /*input_visible_rect=*/decoder_visible_rect,
        output_size ? *output_size : decoder_visible_rect.size(),
        estimated_num_buffers_for_renderer_);
  } else {
    VLOGF(2) << "Initializing ImageProcessor; max buffers: "
             << estimated_num_buffers_for_renderer_;
    // |output_storage_type| holds the storage type of frames created by
    // |main_frame_pool_|, which are used as the output frames for the image
    // processor.
    // As part of b/277581596, |main_frame_pool_| will be changed to create
    // NativePixmap frames instead of GPU memory buffer frames. There will be
    // points during the transition where |main_frame_pool_| will use different
    // storage types for different platforms. The plan is to migrate the
    // ChromeOS Chrome browser frame pool first. Then later, Linux and ChromeOS
    // ARC. To handle correctly configuring |image_processor| during this
    // transition, the frame pool is being used as the source of truth for the
    // |output_storage_type|.
    // TODO(nhebert): Clean up this comment after the NativePixmap migration
    // completes.
    const VideoFrame::StorageType output_storage_type =
        main_frame_pool_->GetFrameStorageType();
    image_processor = ImageProcessorFactory::CreateWithInputCandidates(
        candidates, /*input_visible_rect=*/decoder_visible_rect,
        output_size ? *output_size : decoder_visible_rect.size(),
        output_storage_type, estimated_num_buffers_for_renderer_,
        decoder_task_runner_,
        base::BindRepeating(&PickRenderableFourcc, renderable_fourccs_),
        base::BindPostTaskToCurrentDefault(
            base::BindRepeating(&VideoDecoderPipeline::OnError,
                                decoder_weak_this_, "ImageProcessor error")));
  }

  if (!image_processor) {
    DVLOGF(2) << "Unable to find ImageProcessor to convert format";
    // TODO(crbug.com/40139291): Make CreateWithInputCandidates return an Or
    // type.
    return CroStatus::Codes::kFailedToCreateImageProcessor;
  }

  if (need_aux_frame_pool) {
    // Initialize the auxiliary frame pool with the input format of the image
    // processor. Same as before, this might need to allocate buffers
    // internally, but only to serve the codec needs, hence |num_pictures| is
    // just the number of codec reference frames, plus one to serve the video
    // destination.
    auxiliary_frame_pool_ = std::make_unique<PlatformVideoFramePool>();
    // Use here |num_codec_reference_frames| + 2: one to account for the frame
    // being decoded and an extra one for the ImageProcessor.
    const size_t num_pictures = num_codec_reference_frames + 2;

    // If maximum frame pool size is less than the number of codec reference
    // frames |num_pictures|, the decode will stall. Instead, this returns an
    // error.
    if (num_pictures > GetDecoderMaxOutputFramePoolSize()) {
      return CroStatus::Codes::kInsufficientFramePoolSize;
    }

    VLOGF(2) << "Initializing auxiliary frame pool with up to " << num_pictures
             << " VideoFrames";
    auxiliary_frame_pool_->set_parent_task_runner(decoder_task_runner_);

#if BUILDFLAG(IS_LINUX)
    // The custom allocator creates frames backed by NativePixmap, which uses a
    // VideoFrame::StorageType of VideoFrame::STORAGE_DMABUFS.
    auxiliary_frame_pool_->AsPlatformVideoFramePool()->SetCustomFrameAllocator(
        *allocator, VideoFrame::STORAGE_DMABUFS);
#endif

    CroStatus::Or<GpuBufferLayout> status_or_layout =
        auxiliary_frame_pool_->Initialize(
            image_processor->input_config().fourcc,
            image_processor->input_config().size, decoder_visible_rect,
            decoder_natural_size, num_pictures, use_protected);
    if (!status_or_layout.has_value()) {
      // A PlatformVideoFramePool should never abort initialization.
      DCHECK_NE(status_or_layout.code(), CroStatus::Codes::kResetRequired);
      DVLOGF(2) << "Could not initialize the auxiliary frame pool";
      return std::move(status_or_layout).error();
    }
  }
  // Note that fourcc is specified in ImageProcessor's factory method.
  auto fourcc = image_processor->input_config().fourcc;
  auto size = image_processor->input_config().size;
  size_t num_buffers;
  // We need to instantiate an ImageProcessor with a pool large enough to serve
  // the Renderer pipeline, it should be enough to use
  // |estimated_num_buffers_for_renderer_|. Experimentally it is not enough for
  // some ARM devices that are using ImageProcessor (b/264212288), hence set the
  // max from |estimated_num_buffers_for_renderer_| and empirically chosen
  // kMinImageProcessorOutputFramePoolSize.
  // TODO(b/270990622): Add VD renderer buffer count parameter and plumb it back
  // to clients
#if BUILDFLAG(USE_V4L2_CODEC)
  const size_t kMinImageProcessorOutputFramePoolSize = 10;
  num_buffers = std::max<size_t>(estimated_num_buffers_for_renderer_,
                                 kMinImageProcessorOutputFramePoolSize);
#else
  num_buffers = estimated_num_buffers_for_renderer_;
#endif

  // TODO(b/203240043): Verify that if we're using the image processor for tiled
  // to linear transformation, that the created frame pool is of linear format.
  // TODO(b/203240043): Add CHECKs to verify that the image processor is being
  // created for only valid use cases. Writing to a linear output buffer, e.g.
  VLOGF(2) << "Initializing Image Processor frame pool with up to "
           << num_buffers << " VideoFrames";
  auto status_or_image_processor = ImageProcessorWithPool::Create(
      std::move(image_processor), main_frame_pool_.get(), num_buffers,
      use_protected, decoder_task_runner_);
  if (!status_or_image_processor.has_value()) {
    DVLOGF(2) << "Unable to create ImageProcessorWithPool.";
    return std::move(status_or_image_processor).error();
  }

  image_processor_ = std::move(status_or_image_processor).value();
  VLOGF(2) << "ImageProcessor is created: " << image_processor_->backend_type();
  if (decoder_)
    decoder_->SetDmaIncoherentV4L2(image_processor_->SupportsIncoherentBufs());

  // TODO(b/203240043): Currently, the modifier is not read by any callers of
  // this function. We can eventually provide it by making it available to fetch
  // through the |image_processor|.
  return PixelLayoutCandidate{fourcc, size,
                              gfx::NativePixmapHandle::kNoModifier};
}

#if BUILDFLAG(IS_CHROMEOS)
void VideoDecoderPipeline::OnBufferTranscrypted(
    scoped_refptr<DecoderBuffer> transcrypted_buffer,
    DecodeCB decode_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DCHECK(!has_error_);
  if (!transcrypted_buffer) {
    OnError("Error in buffer transcryption");
    std::move(decode_callback).Run(DecoderStatus::Codes::kFailed);
    return;
  }

  if (drop_transcrypted_buffers_) {
    std::move(decode_callback).Run(DecoderStatus::Codes::kAborted);
    return;
  }

  decoder_->Decode(std::move(transcrypted_buffer), std::move(decode_callback));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace media

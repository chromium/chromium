// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/video_decoder_pipeline.h"

#include <memory>

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "media/base/async_destroy_video_decoder.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/limits.h"
#include "media/base/media_log.h"
#include "media/gpu/chromeos/dmabuf_video_frame_pool.h"
#include "media/gpu/chromeos/image_processor.h"
#include "media/gpu/chromeos/image_processor_factory.h"
#include "media/gpu/chromeos/platform_video_frame_pool.h"
#include "media/gpu/macros.h"
#include "media/media_buildflags.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(USE_VAAPI)
#include "media/gpu/vaapi/vaapi_video_decoder.h"
#elif BUILDFLAG(USE_V4L2_CODEC)
#include "media/gpu/v4l2/v4l2_video_decoder.h"
#else
#error Either VA-API or V4L2 must be used for decode acceleration on Chrome OS.
#endif

namespace media {
namespace {

// The number of requested frames used for the image processor should be the
// number of frames in media::Pipeline plus the current processing frame.
constexpr size_t kNumFramesForImageProcessor = limits::kMaxVideoFrames + 1;

// Preferred output formats in order of preference.
// TODO(mcasas): query the platform for its preferred formats and modifiers.
constexpr Fourcc::Value kPreferredRenderableFourccs[] = {
    Fourcc::NV12,
    Fourcc::YV12,
    Fourcc::P010,
};

// Picks the preferred compositor renderable format from |candidates|, if any.
// If |preferred_fourcc| is provided, contained in |candidates|, and considered
// renderable, it returns that. Otherwise, it goes through
// |kPreferredRenderableFourccs| until it finds one that's in |candidates|. If
// it can't find a renderable format in |candidates|, it returns absl::nullopt.
absl::optional<Fourcc> PickRenderableFourcc(
    const std::vector<Fourcc>& candidates,
    absl::optional<Fourcc> preferred_fourcc) {
  if (preferred_fourcc && base::Contains(candidates, *preferred_fourcc)) {
    for (const auto value : kPreferredRenderableFourccs) {
      if (Fourcc(value) == *preferred_fourcc)
        return preferred_fourcc;
    }
  }
  for (const auto value : kPreferredRenderableFourccs) {
    if (base::Contains(candidates, Fourcc(value)))
      return Fourcc(value);
  }
  return absl::nullopt;
}

}  //  namespace

VideoDecoderMixin::VideoDecoderMixin(
    std::unique_ptr<MediaLog> media_log,
    scoped_refptr<base::SequencedTaskRunner> decoder_task_runner,
    base::WeakPtr<VideoDecoderMixin::Client> client)
    : decoder_task_runner_(std::move(decoder_task_runner)),
      client_(std::move(client)) {}

VideoDecoderMixin::~VideoDecoderMixin() = default;

bool VideoDecoderMixin::NeedsTranscryption() {
  return false;
}

// static
std::unique_ptr<VideoDecoder> VideoDecoderPipeline::Create(
    scoped_refptr<base::SequencedTaskRunner> client_task_runner,
    std::unique_ptr<DmabufVideoFramePool> frame_pool,
    std::unique_ptr<VideoFrameConverter> frame_converter,
    std::unique_ptr<MediaLog> media_log) {
  DCHECK(client_task_runner);
  DCHECK(frame_pool);
  DCHECK(frame_converter);

  CreateDecoderFunctionCB create_decoder_function_cb =
#if BUILDFLAG(USE_VAAPI)
      base::BindOnce(&VaapiVideoDecoder::Create);
#elif BUILDFLAG(USE_V4L2_CODEC)
      base::BindOnce(&V4L2VideoDecoder::Create);
#endif

  auto* pipeline = new VideoDecoderPipeline(
      std::move(client_task_runner), std::move(frame_pool),
      std::move(frame_converter), std::move(media_log),
      std::move(create_decoder_function_cb));
  return std::make_unique<AsyncDestroyVideoDecoder<VideoDecoderPipeline>>(
      base::WrapUnique(pipeline));
}

// static
absl::optional<SupportedVideoDecoderConfigs>
VideoDecoderPipeline::GetSupportedConfigs(
    const gpu::GpuDriverBugWorkarounds& workarounds) {
  absl::optional<SupportedVideoDecoderConfigs> configs =
#if BUILDFLAG(USE_VAAPI)
      VaapiVideoDecoder::GetSupportedConfigs();
#elif BUILDFLAG(USE_V4L2_CODEC)
      V4L2VideoDecoder::GetSupportedConfigs();
#endif

  if (!configs)
    return absl::nullopt;

  if (workarounds.disable_accelerated_vp8_decode) {
    base::EraseIf(configs.value(), [](const auto& config) {
      return config.profile_min >= VP8PROFILE_MIN &&
             config.profile_max <= VP8PROFILE_MAX;
    });
  }

  if (workarounds.disable_accelerated_vp9_decode) {
    base::EraseIf(configs.value(), [](const auto& config) {
      return config.profile_min >= VP9PROFILE_PROFILE0 &&
             config.profile_max <= VP9PROFILE_PROFILE0;
    });
  }

  if (workarounds.disable_accelerated_vp9_profile2_decode) {
    base::EraseIf(configs.value(), [](const auto& config) {
      return config.profile_min >= VP9PROFILE_PROFILE2 &&
             config.profile_max <= VP9PROFILE_PROFILE2;
    });
  }

  return configs;
}

VideoDecoderPipeline::VideoDecoderPipeline(
    scoped_refptr<base::SequencedTaskRunner> client_task_runner,
    std::unique_ptr<DmabufVideoFramePool> frame_pool,
    std::unique_ptr<VideoFrameConverter> frame_converter,
    std::unique_ptr<MediaLog> media_log,
    CreateDecoderFunctionCB create_decoder_function_cb)
    : client_task_runner_(std::move(client_task_runner)),
      decoder_task_runner_(base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::WithBaseSyncPrimitives(), base::TaskPriority::USER_VISIBLE},
          base::SingleThreadTaskRunnerThreadMode::DEDICATED)),
      main_frame_pool_(std::move(frame_pool)),
      frame_converter_(std::move(frame_converter)),
      media_log_(std::move(media_log)),
      create_decoder_function_cb_(std::move(create_decoder_function_cb)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  DETACH_FROM_SEQUENCE(decoder_sequence_checker_);
  DCHECK(main_frame_pool_);
  DCHECK(frame_converter_);
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

  main_frame_pool_.reset();
  frame_converter_.reset();
  decoder_.reset();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  buffer_transcryptor_.reset();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
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
  // TODO(mcasas): query |decoder_| instead.
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

  // TODO(mcasas): query |decoder_| instead.
  return 4;
}

bool VideoDecoderPipeline::NeedsBitstreamConversion() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  // TODO(mcasas): also query |decoder_|.
  return needs_bitstream_conversion_;
}

bool VideoDecoderPipeline::CanReadWithoutStalling() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  // TODO(mcasas): also query |decoder_|.
  return main_frame_pool_ && !main_frame_pool_->IsExhausted();
}

void VideoDecoderPipeline::Initialize(const VideoDecoderConfig& config,
                                      bool /* low_delay */,
                                      CdmContext* cdm_context,
                                      InitCB init_cb,
                                      const OutputCB& output_cb,
                                      const WaitingCB& waiting_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  VLOGF(2) << "config: " << config.AsHumanReadableString();

  if (!config.IsValidConfig()) {
    VLOGF(1) << "config is not valid";
    std::move(init_cb).Run(StatusCode::kDecoderUnsupportedConfig);
    return;
  }
#if BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)
  if (config.is_encrypted() && !cdm_context) {
    VLOGF(1) << "Encrypted streams require a CdmContext";
    std::move(init_cb).Run(StatusCode::kDecoderUnsupportedConfig);
    return;
  }
#else   // BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)
  if (config.is_encrypted() && !allow_encrypted_content_for_testing_) {
    VLOGF(1) << "Encrypted streams are not supported for this VD";
    std::move(init_cb).Run(StatusCode::kEncryptedContentUnsupported);
    return;
  }
  if (cdm_context && !allow_encrypted_content_for_testing_) {
    VLOGF(1) << "cdm_context is not supported.";
    std::move(init_cb).Run(StatusCode::kEncryptedContentUnsupported);
    return;
  }
#endif  // !BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)

  needs_bitstream_conversion_ = (config.codec() == VideoCodec::kH264) ||
                                (config.codec() == VideoCodec::kHEVC);

  decoder_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoDecoderPipeline::InitializeTask, decoder_weak_this_,
                     config, cdm_context, std::move(init_cb),
                     std::move(output_cb), std::move(waiting_cb)));
}

void VideoDecoderPipeline::InitializeTask(const VideoDecoderConfig& config,
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
    decoder_ =
        std::move(create_decoder_function_cb_)
            .Run(media_log_->Clone(), decoder_task_runner_, decoder_weak_this_);
  }
  // Note: |decoder_| might fail to be created, e.g. on V4L2 platforms.
  if (!decoder_) {
    OnError("|decoder_| creation failed.");
    client_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(init_cb), StatusCode::kDecoderFailedCreation));
    return;
  }

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
                                            Status status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(4) << "Initialization status = " << status.code();

  if (!status.is_ok()) {
    MEDIA_LOG(ERROR, media_log_)
        << "VideoDecoderPipeline |decoder_| Initialize() failed, status: "
        << status.code();
    decoder_ = nullptr;
  }
  MEDIA_LOG(INFO, media_log_)
      << "VideoDecoderPipeline |decoder_| Initialize() successful";

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (decoder_ && decoder_->NeedsTranscryption()) {
    if (!cdm_context) {
      VLOGF(1) << "CdmContext required for transcryption";
      decoder_ = nullptr;
      status = Status(StatusCode::kDecoderMissingCdmForEncryptedContent);
    } else {
      // We need to enable transcryption for protected content.
      buffer_transcryptor_ = std::make_unique<DecoderBufferTranscryptor>(
          cdm_context,
          base::BindRepeating(&VideoDecoderPipeline::OnBufferTranscrypted,
                              decoder_weak_this_),
          base::BindRepeating(&VideoDecoderPipeline::OnDecoderWaiting,
                              decoder_weak_this_));
    }
  } else {
    // In case this was created on a prior initialization but no longer needed.
    buffer_transcryptor_.reset();
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (buffer_transcryptor_)
    buffer_transcryptor_->Reset(DecodeStatus::ABORTED);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  CallFlushCbIfNeeded(DecodeStatus::ABORTED);

  if (need_frame_pool_rebuild_) {
    need_frame_pool_rebuild_ = false;
    if (main_frame_pool_)
      main_frame_pool_->ReleaseAllFrames();
    if (auxiliary_frame_pool_)
      auxiliary_frame_pool_->ReleaseAllFrames();
  }

  client_task_runner_->PostTask(FROM_HERE, std::move(reset_cb));
}

void VideoDecoderPipeline::Decode(scoped_refptr<DecoderBuffer> buffer,
                                  DecodeCB decode_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  DVLOGF(4);

  decoder_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoDecoderPipeline::DecodeTask, decoder_weak_this_,
                     std::move(buffer), std::move(decode_cb)));
}

void VideoDecoderPipeline::DecodeTask(scoped_refptr<DecoderBuffer> buffer,
                                      DecodeCB decode_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DCHECK(decoder_);
  DVLOGF(4);

  if (has_error_) {
    client_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(decode_cb),
                                  Status(DecodeStatus::DECODE_ERROR)));
    return;
  }

  const bool is_flush = buffer->end_of_stream();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (buffer_transcryptor_) {
    buffer_transcryptor_->EnqueueBuffer(
        std::move(buffer),
        base::BindOnce(&VideoDecoderPipeline::OnDecodeDone, decoder_weak_this_,
                       is_flush, std::move(decode_cb)));
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  decoder_->Decode(
      std::move(buffer),
      base::BindOnce(&VideoDecoderPipeline::OnDecodeDone, decoder_weak_this_,
                     is_flush, std::move(decode_cb)));
}

void VideoDecoderPipeline::OnDecodeDone(bool is_flush,
                                        DecodeCB decode_cb,
                                        Status status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(4) << "is_flush: " << is_flush << ", status: " << status.code();

  if (has_error_)
    status = Status(DecodeStatus::DECODE_ERROR);

  if (is_flush && status.is_ok()) {
    client_flush_cb_ = std::move(decode_cb);
    CallFlushCbIfNeeded(DecodeStatus::OK);
    return;
  }

  client_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(decode_cb), std::move(status)));
}

void VideoDecoderPipeline::OnFrameDecoded(scoped_refptr<VideoFrame> frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DCHECK(frame_converter_);
  DVLOGF(4);

  if (image_processor_) {
    image_processor_->Process(
        std::move(frame),
        base::BindOnce(&VideoDecoderPipeline::OnFrameProcessed,
                       decoder_weak_this_));
  } else {
    frame_converter_->ConvertFrame(std::move(frame));
  }
}

void VideoDecoderPipeline::OnFrameProcessed(scoped_refptr<VideoFrame> frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DCHECK(frame_converter_);
  DVLOGF(4);

  frame_converter_->ConvertFrame(std::move(frame));
}

void VideoDecoderPipeline::OnFrameConverted(scoped_refptr<VideoFrame> frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(4);

  if (!frame)
    return OnError("Frame converter returns null frame.");
  if (has_error_) {
    DVLOGF(2) << "Skip returning frames after error occurs.";
    return;
  }

  // Flag that the video frame is capable of being put in an overlay.
  frame->metadata().allow_overlay = true;
  // Flag that the video frame was decoded in a power efficient way.
  frame->metadata().power_efficient = true;

  // MojoVideoDecoderService expects the |output_cb_| to be called on the client
  // task runner, even though media::VideoDecoder states frames should be output
  // without any thread jumping.
  // Note that all the decode/flush/output/reset callbacks are executed on
  // |client_task_runner_|.
  client_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(client_output_cb_, std::move(frame)));

  // After outputting a frame, flush might be completed.
  CallFlushCbIfNeeded(DecodeStatus::OK);
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
  DVLOGF(3);
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);

  return frame_converter_->HasPendingFrames() ||
         (image_processor_ && image_processor_->HasPendingFrames());
}

void VideoDecoderPipeline::OnError(const std::string& msg) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  VLOGF(1) << msg;
  MEDIA_LOG(ERROR, media_log_) << "VideoDecoderPipeline " << msg;

  has_error_ = true;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (buffer_transcryptor_)
    buffer_transcryptor_->Reset(DecodeStatus::DECODE_ERROR);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  CallFlushCbIfNeeded(DecodeStatus::DECODE_ERROR);
}

void VideoDecoderPipeline::CallFlushCbIfNeeded(DecodeStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(3) << "status: " << status;

  if (!client_flush_cb_)
    return;

  // Flush is not completed yet.
  if (status == DecodeStatus::OK && HasPendingFrames())
    return;

  client_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(client_flush_cb_), status));
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
  DVLOGF(3);

  if (need_apply_new_resolution && !HasPendingFrames()) {
    need_apply_new_resolution = false;
    decoder_->ApplyResolutionChange();
  }
}

DmabufVideoFramePool* VideoDecoderPipeline::GetVideoFramePool() const {
  DVLOGF(3);
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);

  // TODO(andrescj): consider returning a WeakPtr instead. That way, if callers
  // store the returned pointer, they know that they should check it's valid
  // because the video frame pool can change across resolution changes if we go
  // from using an image processor to not using one (or viceversa).
  if (image_processor_)
    return auxiliary_frame_pool_.get();
  return main_frame_pool_.get();
}

StatusOr<std::pair<Fourcc, gfx::Size>>
VideoDecoderPipeline::PickDecoderOutputFormat(
    const std::vector<std::pair<Fourcc, gfx::Size>>& candidates,
    const gfx::Rect& decoder_visible_rect,
    const gfx::Size& decoder_natural_size,
    absl::optional<gfx::Size> output_size,
    size_t num_of_pictures,
    bool use_protected,
    bool need_aux_frame_pool) {
  DVLOGF(3);
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);

  if (candidates.empty())
    return Status(StatusCode::kInvalidArgument);

  auxiliary_frame_pool_.reset();
  image_processor_.reset();

  // As long as we're not scaling, check if any of the |candidates| formats is
  // directly renderable.
  if (!output_size || *output_size == decoder_visible_rect.size()) {
    for (const auto preferred_fourcc : kPreferredRenderableFourccs) {
      for (const auto& candidate : candidates) {
        if (candidate.first == Fourcc(preferred_fourcc)) {
          StatusOr<GpuBufferLayout> status_or_layout =
              main_frame_pool_->Initialize(
                  candidate.first, candidate.second, decoder_visible_rect,
                  decoder_natural_size, num_of_pictures, use_protected);
          return status_or_layout.has_error()
                     ? StatusOr<std::pair<Fourcc, gfx::Size>>(
                           std::move(status_or_layout).error())
                     : StatusOr<std::pair<Fourcc, gfx::Size>>(candidate);
        }
      }
    }
  }

  std::unique_ptr<ImageProcessor> image_processor =
      ImageProcessorFactory::CreateWithInputCandidates(
          candidates, /*input_visible_rect=*/decoder_visible_rect,
          output_size ? *output_size : decoder_visible_rect.size(),
          kNumFramesForImageProcessor, decoder_task_runner_,
          base::BindRepeating(&PickRenderableFourcc),
          BindToCurrentLoop(base::BindRepeating(&VideoDecoderPipeline::OnError,
                                                decoder_weak_this_,
                                                "ImageProcessor error")));
  if (!image_processor) {
    DVLOGF(2) << "Unable to find ImageProcessor to convert format";
    return Status(StatusCode::kInvalidArgument);
  }

  if (need_aux_frame_pool) {
    // Initialize the auxiliary frame pool with the input format of the image
    // processor. Note that we pass nullptr as the GpuMemoryBufferFactory. That
    // way, the pool will allocate buffers using minigbm directly instead of
    // going through Ozone which means it won't create DRM/KMS framebuffers for
    // those buffers. This is good because these buffers don't end up as
    // overlays anyway.
    auxiliary_frame_pool_ = std::make_unique<PlatformVideoFramePool>(
        /*gpu_memory_buffer_factory=*/nullptr);
    auxiliary_frame_pool_->set_parent_task_runner(decoder_task_runner_);
    StatusOr<GpuBufferLayout> status_or_layout =
        auxiliary_frame_pool_->Initialize(
            image_processor->input_config().fourcc,
            image_processor->input_config().size, decoder_visible_rect,
            decoder_natural_size, num_of_pictures, use_protected);
    if (status_or_layout.has_error()) {
      // A PlatformVideoFramePool should never abort initialization.
      DCHECK_NE(status_or_layout.code(), StatusCode::kAborted);
      DVLOGF(2) << "Could not initialize the auxiliary frame pool";
      return Status(StatusCode::kInvalidArgument);
    }
  }

  // Note that fourcc is specified in ImageProcessor's factory method.
  auto fourcc = image_processor->input_config().fourcc;
  auto size = image_processor->input_config().size;

  // Setup new pipeline.
  auto status_or_image_processor = ImageProcessorWithPool::Create(
      std::move(image_processor), main_frame_pool_.get(),
      kNumFramesForImageProcessor, use_protected, decoder_task_runner_);
  if (status_or_image_processor.has_error()) {
    DVLOGF(2) << "Unable to create ImageProcessorWithPool.";
    return std::move(status_or_image_processor).error();
  }

  image_processor_ = std::move(status_or_image_processor).value();
  return std::make_pair(fourcc, size);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void VideoDecoderPipeline::OnBufferTranscrypted(
    scoped_refptr<DecoderBuffer> transcrypted_buffer,
    DecodeCB decode_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DCHECK(!has_error_);
  if (!transcrypted_buffer) {
    OnError("Error in buffer transcryption");
    std::move(decode_callback).Run(DecodeStatus::DECODE_ERROR);
    return;
  }

  decoder_->Decode(std::move(transcrypted_buffer), std::move(decode_callback));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace media

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/video_decoder_pipeline.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "media/base/async_destroy_video_decoder.h"
#include "media/base/limits.h"
#include "media/base/media_log.h"
#include "media/gpu/chromeos/dmabuf_video_frame_pool.h"
#include "media/gpu/chromeos/image_processor.h"
#include "media/gpu/chromeos/image_processor_factory.h"
#include "media/gpu/chromeos/platform_video_frame_pool.h"
#include "media/gpu/macros.h"
#include "media/media_buildflags.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace media {
namespace {

// The number of requested frames used for the image processor should be the
// number of frames in media::Pipeline plus the current processing frame.
constexpr size_t kNumFramesForImageProcessor = limits::kMaxVideoFrames + 1;

// Pick a compositor renderable format from |candidates|.
// Return zero if not found.
absl::optional<Fourcc> PickRenderableFourcc(
    const std::vector<Fourcc>& candidates) {
  // Hardcode compositor renderable format now.
  // TODO: figure out a way to pick the best one dynamically.
  // Prefer YVU420 and NV12 because ArcGpuVideoDecodeAccelerator only supports
  // single physical plane.
  constexpr Fourcc::Value kPreferredFourccValues[] = {
#if defined(ARCH_CPU_ARM_FAMILY)
    Fourcc::NV12,
    Fourcc::YV12,
#endif
    // For kepler.
    Fourcc::AR24,
  };

  for (const auto& value : kPreferredFourccValues) {
    if (std::find(candidates.begin(), candidates.end(), Fourcc(value)) !=
        candidates.end()) {
      return Fourcc(value);
    }
  }
  return absl::nullopt;
}

}  //  namespace

DecoderInterface::DecoderInterface(
    scoped_refptr<base::SequencedTaskRunner> decoder_task_runner,
    base::WeakPtr<DecoderInterface::Client> client)
    : decoder_task_runner_(std::move(decoder_task_runner)),
      client_(std::move(client)) {}
DecoderInterface::~DecoderInterface() = default;

// static
std::unique_ptr<VideoDecoder> VideoDecoderPipeline::Create(
    scoped_refptr<base::SequencedTaskRunner> client_task_runner,
    std::unique_ptr<DmabufVideoFramePool> frame_pool,
    std::unique_ptr<VideoFrameConverter> frame_converter,
    std::unique_ptr<MediaLog> /*media_log*/,
    CreateDecoderFunctionCB create_decoder_function_cb) {
  if (!client_task_runner || !frame_pool || !frame_converter) {
    VLOGF(1) << "One of arguments is nullptr.";
    return nullptr;
  }

  auto* decoder = new VideoDecoderPipeline(
      std::move(client_task_runner), std::move(frame_pool),
      std::move(frame_converter), std::move(create_decoder_function_cb));
  return std::make_unique<AsyncDestroyVideoDecoder<VideoDecoderPipeline>>(
      base::WrapUnique(decoder));
}

VideoDecoderPipeline::VideoDecoderPipeline(
    scoped_refptr<base::SequencedTaskRunner> client_task_runner,
    std::unique_ptr<DmabufVideoFramePool> frame_pool,
    std::unique_ptr<VideoFrameConverter> frame_converter,
    CreateDecoderFunctionCB create_decoder_function_cb)
    : client_task_runner_(std::move(client_task_runner)),
      decoder_task_runner_(base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::WithBaseSyncPrimitives(), base::TaskPriority::USER_VISIBLE},
          base::SingleThreadTaskRunnerThreadMode::DEDICATED)),
      main_frame_pool_(std::move(frame_pool)),
      frame_converter_(std::move(frame_converter)),
      create_decoder_function_cb_(std::move(create_decoder_function_cb)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  DETACH_FROM_SEQUENCE(decoder_sequence_checker_);
  DCHECK(main_frame_pool_);
  DCHECK(frame_converter_);
  DCHECK(client_task_runner_);
  DVLOGF(2);

  client_weak_this_ = client_weak_this_factory_.GetWeakPtr();
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
}

void VideoDecoderPipeline::DestroyAsync(
    std::unique_ptr<VideoDecoderPipeline> decoder) {
  DVLOGF(2);
  DCHECK(decoder);
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder->client_sequence_checker_);

  decoder->client_weak_this_factory_.InvalidateWeakPtrs();
  auto* decoder_task_runner = decoder->decoder_task_runner_.get();
  decoder_task_runner->DeleteSoon(FROM_HERE, std::move(decoder));
}

VideoDecoderType VideoDecoderPipeline::GetDecoderType() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
#if BUILDFLAG(USE_VAAPI)
  return VideoDecoderType::kVaapi;
#elif BUILDFLAG(USE_V4L2_CODEC)
  return VideoDecoderType::kV4L2;
#endif
  return VideoDecoderType::kUnknown;
}

bool VideoDecoderPipeline::IsPlatformDecoder() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  return true;
}

int VideoDecoderPipeline::GetMaxDecodeRequests() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  return 4;
}

bool VideoDecoderPipeline::NeedsBitstreamConversion() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  return needs_bitstream_conversion_;
}

bool VideoDecoderPipeline::CanReadWithoutStalling() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

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
  if (config.is_encrypted()) {
    VLOGF(1) << "Encrypted streams are not supported for this VD";
    std::move(init_cb).Run(StatusCode::kEncryptedContentUnsupported);
    return;
  }
  if (cdm_context) {
    VLOGF(1) << "cdm_context is not supported.";
    std::move(init_cb).Run(StatusCode::kEncryptedContentUnsupported);
    return;
  }
#endif  // !BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)

  needs_bitstream_conversion_ =
      (config.codec() == kCodecH264) || (config.codec() == kCodecHEVC);

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
  DCHECK(!init_cb_);

  client_output_cb_ = std::move(output_cb);
  init_cb_ = std::move(init_cb);

  // Initialize() and correspondingly InitializeTask(), are called both on first
  // initialization and on subsequent stream |config| changes, e.g. change of
  // resolution. Subsequent initializations are marked by |decoder_| already
  // existing.
  if (!decoder_) {
    decoder_ = create_decoder_function_cb_.Run(decoder_task_runner_,
                                               decoder_weak_this_);

    if (!decoder_) {
      DVLOGF(2) << "|decoder_| creation failed.";
      client_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(std::move(init_cb_),
                                    StatusCode::kDecoderFailedCreation));
      return;
    }
  }

  decoder_->Initialize(
      config, cdm_context,
      base::BindOnce(&VideoDecoderPipeline::OnInitializeDone,
                     decoder_weak_this_),
      base::BindRepeating(&VideoDecoderPipeline::OnFrameDecoded,
                          decoder_weak_this_),
      waiting_cb);
}

void VideoDecoderPipeline::OnInitializeDone(Status status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DCHECK(init_cb_);
  DVLOGF(4) << "Initialization status = " << status.code();

  if (!status.is_ok())
    decoder_ = nullptr;

  client_task_runner_->PostTask(FROM_HERE,
                                base::BindOnce(std::move(init_cb_), status));
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

  CallFlushCbIfNeeded(DecodeStatus::ABORTED);

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

  bool is_flush = buffer->end_of_stream();
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

bool VideoDecoderPipeline::HasPendingFrames() const {
  DVLOGF(3);
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);

  return frame_converter_->HasPendingFrames() ||
         (image_processor_ && image_processor_->HasPendingFrames());
}

void VideoDecoderPipeline::OnError(const std::string& msg) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  VLOGF(1) << msg;

  has_error_ = true;
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

  // |main_frame_pool_| is used by |image_processor_| in this case.
  // |decoder_| will output native buffer allocated by itself.
  // (e.g. V4L2 MMAP buffer in V4L2 API and VA surface in VA API.)
  if (image_processor_)
    return nullptr;
  return main_frame_pool_.get();
}

absl::optional<std::pair<Fourcc, gfx::Size>>
VideoDecoderPipeline::PickDecoderOutputFormat(
    const std::vector<std::pair<Fourcc, gfx::Size>>& candidates,
    const gfx::Rect& visible_rect) {
  DVLOGF(3);
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);

  if (candidates.empty())
    return absl::nullopt;

  image_processor_.reset();

  // Check if any candidate format is renderable without the need of
  // ImageProcessor.
  std::vector<Fourcc> fourccs;
  for (const auto& candidate : candidates)
    fourccs.push_back(candidate.first);
  const auto renderable_fourcc = PickRenderableFourcc(fourccs);
  if (renderable_fourcc) {
    for (const auto& candidate : candidates)
      if (candidate.first == renderable_fourcc)
        return candidate;
    DVLOGF(2) << "Renderable Fourcc not in candidates list. This is a bug.";
    return absl::nullopt;
  }

  std::unique_ptr<ImageProcessor> image_processor =
      ImageProcessorFactory::CreateWithInputCandidates(
          candidates, visible_rect.size(), kNumFramesForImageProcessor,
          decoder_task_runner_, base::BindRepeating(&PickRenderableFourcc),
          base::BindRepeating(&VideoDecoderPipeline::OnImageProcessorError,
                              decoder_weak_this_));
  if (!image_processor) {
    DVLOGF(2) << "Unable to find ImageProcessor to convert format";
    return absl::nullopt;
  }

  // Note that fourcc is specified in ImageProcessor's factory method.
  auto fourcc = image_processor->input_config().fourcc;
  auto size = image_processor->input_config().size;

  // Setup new pipeline.
  image_processor_ = ImageProcessorWithPool::Create(
      std::move(image_processor), main_frame_pool_.get(),
      kNumFramesForImageProcessor, decoder_task_runner_);
  if (!image_processor_) {
    DVLOGF(2) << "Unable to create ImageProcessorWithPool.";
    return absl::nullopt;
  }

  return std::make_pair(fourcc, size);
}

void VideoDecoderPipeline::OnImageProcessorError() {
  VLOGF(1);
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);

  client_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VideoDecoderPipeline::OnError,
                                client_weak_this_, "Image processor error"));
}

}  // namespace media

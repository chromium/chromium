// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/video_decoder_pipeline.h"
#include <memory>

#include "base/bind.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "media/gpu/chromeos/dmabuf_video_frame_pool.h"
#include "media/gpu/chromeos/platform_video_frame_pool.h"
#include "media/gpu/macros.h"

namespace media {

// static
std::unique_ptr<VideoDecoder> VideoDecoderPipeline::Create(
    scoped_refptr<base::SequencedTaskRunner> client_task_runner,
    std::unique_ptr<DmabufVideoFramePool> frame_pool,
    std::unique_ptr<VideoFrameConverter> frame_converter,
    gpu::GpuMemoryBufferFactory* const gpu_memory_buffer_factory,
    GetCreateVDFunctionsCB get_create_vd_functions_cb) {
  if (!client_task_runner || !frame_pool || !frame_converter) {
    VLOGF(1) << "One of arguments is nullptr.";
    return nullptr;
  }

  if (get_create_vd_functions_cb.Run(nullptr).empty()) {
    VLOGF(1) << "No available function to create video decoder.";
    return nullptr;
  }

  return base::WrapUnique<VideoDecoder>(new VideoDecoderPipeline(
      std::move(client_task_runner), std::move(frame_pool),
      std::move(frame_converter), gpu_memory_buffer_factory,
      std::move(get_create_vd_functions_cb)));
}

VideoDecoderPipeline::VideoDecoderPipeline(
    scoped_refptr<base::SequencedTaskRunner> client_task_runner,
    std::unique_ptr<DmabufVideoFramePool> frame_pool,
    std::unique_ptr<VideoFrameConverter> frame_converter,
    gpu::GpuMemoryBufferFactory* const gpu_memory_buffer_factory,
    GetCreateVDFunctionsCB get_create_vd_functions_cb)
    : client_task_runner_(std::move(client_task_runner)),
      decoder_task_runner_(base::CreateSingleThreadTaskRunner(
          {base::ThreadPool(), base::WithBaseSyncPrimitives(),
           base::TaskPriority::USER_VISIBLE},
          base::SingleThreadTaskRunnerThreadMode::DEDICATED)),
      main_frame_pool_(std::move(frame_pool)),
      gpu_memory_buffer_factory_(gpu_memory_buffer_factory),
      frame_converter_(std::move(frame_converter)),
      get_create_vd_functions_cb_(std::move(get_create_vd_functions_cb)) {
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
  // We have to destroy |main_frame_pool_| on |decoder_task_runner_|, so the
  // destructor is also called on |decoder_task_runner_|.
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(3);
}

void VideoDecoderPipeline::Destroy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  DVLOGF(2);

  client_weak_this_factory_.InvalidateWeakPtrs();

  decoder_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VideoDecoderPipeline::DestroyTask,
                                base::Unretained(this)));
}

void VideoDecoderPipeline::DestroyTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(3);

  decoder_weak_this_factory_.InvalidateWeakPtrs();

  // The frame pool and converter should be destroyed on |decoder_task_runner_|.
  main_frame_pool_.reset();
  frame_converter_.reset();

  decoder_.reset();
  used_create_vd_func_ = nullptr;

  delete this;
}

std::string VideoDecoderPipeline::GetDisplayName() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);

  return "VideoDecoderPipeline";
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
                                      bool low_delay,
                                      CdmContext* cdm_context,
                                      InitCB init_cb,
                                      const OutputCB& output_cb,
                                      const WaitingCB& waiting_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  VLOGF(2) << "config: " << config.AsHumanReadableString();

  if (!config.IsValidConfig()) {
    VLOGF(1) << "config is not valid";
    std::move(init_cb).Run(false);
    return;
  }
  if (config.is_encrypted()) {
    VLOGF(1) << "Encrypted streams are not supported for this VD";
    std::move(init_cb).Run(false);
    return;
  }
  if (cdm_context) {
    VLOGF(1) << "cdm_context is not supported.";
    std::move(init_cb).Run(false);
    return;
  }

  needs_bitstream_conversion_ = (config.codec() == kCodecH264);

  decoder_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoDecoderPipeline::InitializeTask, decoder_weak_this_,
                     config, std::move(init_cb), std::move(output_cb)));
}

void VideoDecoderPipeline::InitializeTask(const VideoDecoderConfig& config,
                                          InitCB init_cb,
                                          const OutputCB& output_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DCHECK(!init_cb_);

  client_output_cb_ = std::move(output_cb);
  init_cb_ = std::move(init_cb);
  base::queue<VideoDecoderPipeline::CreateVDFunc> create_vd_funcs =
      get_create_vd_functions_cb_.Run(used_create_vd_func_);

  if (!decoder_) {
    CreateAndInitializeVD(std::move(create_vd_funcs), config);
  } else {
    decoder_->Initialize(
        config,
        // If it fails to re-initialize current |decoder_|, it will create
        // another decoder instance by trying available VD creation functions
        // again. See |OnInitializeDone| for detail.
        base::BindOnce(&VideoDecoderPipeline::OnInitializeDone,
                       decoder_weak_this_, std::move(create_vd_funcs), config),
        base::BindRepeating(&VideoDecoderPipeline::OnFrameDecoded,
                            decoder_weak_this_));
  }
}

void VideoDecoderPipeline::CreateAndInitializeVD(
    base::queue<VideoDecoderPipeline::CreateVDFunc> create_vd_funcs,
    VideoDecoderConfig config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DCHECK(init_cb_);
  DCHECK(!decoder_);
  DCHECK(!used_create_vd_func_);
  DVLOGF(3);

  if (create_vd_funcs.empty()) {
    DVLOGF(2) << "No available video decoder.";
    client_task_runner_->PostTask(FROM_HERE,
                                  base::BindOnce(std::move(init_cb_), false));
    return;
  }

  used_create_vd_func_ = create_vd_funcs.front();
  create_vd_funcs.pop();
  decoder_ = used_create_vd_func_(
      decoder_task_runner_,
      base::BindRepeating(&VideoDecoderPipeline::GetVideoFramePool,
                          base::Unretained(this)));
  if (!decoder_) {
    DVLOGF(2) << "Failed to create VideoDecoder.";
    used_create_vd_func_ = nullptr;
    return CreateAndInitializeVD(std::move(create_vd_funcs), config);
  }

  decoder_->Initialize(
      config,
      base::BindOnce(&VideoDecoderPipeline::OnInitializeDone,
                     decoder_weak_this_, std::move(create_vd_funcs), config),
      base::BindRepeating(&VideoDecoderPipeline::OnFrameDecoded,
                          decoder_weak_this_));
}

void VideoDecoderPipeline::OnInitializeDone(
    base::queue<VideoDecoderPipeline::CreateVDFunc> create_vd_funcs,
    VideoDecoderConfig config,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DCHECK(init_cb_);
  DVLOGF(4) << "Initialization " << (success ? "success." : "failure.");

  if (success) {
    DVLOGF(2) << "Initialize VD successfully.";
    client_task_runner_->PostTask(FROM_HERE,
                                  base::BindOnce(std::move(init_cb_), true));
    return;
  }

  DVLOGF(3) << "Reset VD, try the next create function.";
  decoder_ = nullptr;
  used_create_vd_func_ = nullptr;
  CreateAndInitializeVD(std::move(create_vd_funcs), config);
}

void VideoDecoderPipeline::Reset(base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  DVLOGF(3);

  decoder_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VideoDecoderPipeline::ResetTask,
                                decoder_weak_this_, std::move(closure)));
}

void VideoDecoderPipeline::ResetTask(base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DCHECK(decoder_);
  DCHECK(!client_reset_cb_);
  DVLOGF(3);

  client_reset_cb_ = std::move(closure);
  decoder_->Reset(
      base::BindOnce(&VideoDecoderPipeline::OnResetDone, decoder_weak_this_));
}

void VideoDecoderPipeline::OnResetDone() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DCHECK(client_reset_cb_);
  DVLOGF(3);

  frame_converter_->AbortPendingFrames();

  CallFlushCbIfNeeded(DecodeStatus::ABORTED);

  client_task_runner_->PostTask(FROM_HERE, std::move(client_reset_cb_));
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
                                        DecodeStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(4) << "is_flush: " << is_flush << ", status: " << status;

  if (has_error_)
    status = DecodeStatus::DECODE_ERROR;

  if (is_flush && status == DecodeStatus::OK) {
    client_flush_cb_ = std::move(decode_cb);
    CallFlushCbIfNeeded(DecodeStatus::OK);
    return;
  }

  client_task_runner_->PostTask(FROM_HERE,
                                base::BindOnce(std::move(decode_cb), status));
}

void VideoDecoderPipeline::OnFrameDecoded(scoped_refptr<VideoFrame> frame) {
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
  frame->metadata()->SetBoolean(VideoFrameMetadata::ALLOW_OVERLAY, true);
  // Flag that the video frame was decoded in a power efficient way.
  frame->metadata()->SetBoolean(VideoFrameMetadata::POWER_EFFICIENT, true);

  // MojoVideoDecoderService expects the |output_cb_| to be called on the client
  // task runner, even though media::VideoDecoder states frames should be output
  // without any thread jumping.
  // Note that all the decode/flush/output/reset callbacks are executed on
  // |client_task_runner_|.
  client_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(client_output_cb_, std::move(frame)));

  // After outputting a frame, flush might be completed.
  CallFlushCbIfNeeded(DecodeStatus::OK);
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
  if (status == DecodeStatus::OK && frame_converter_->HasPendingFrames())
    return;

  client_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(client_flush_cb_), status));
}

DmabufVideoFramePool* VideoDecoderPipeline::GetVideoFramePool() const {
  return main_frame_pool_.get();
}

}  // namespace media

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/mac/video_toolbox_video_decoder.h"

#include <VideoToolbox/VideoToolbox.h>

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/memory/scoped_policy.h"
#include "base/task/bind_post_task.h"
#include "media/base/decoder_status.h"
#include "media/base/media_log.h"
#include "media/base/video_frame.h"
#include "media/gpu/accelerated_video_decoder.h"
#include "media/gpu/h264_decoder.h"
#include "media/gpu/mac/video_toolbox_decode_metadata.h"
#include "media/gpu/mac/video_toolbox_h264_accelerator.h"

namespace media {

namespace {

constexpr VideoCodecProfile kSupportedProfiles[] = {
    H264PROFILE_BASELINE,
    H264PROFILE_EXTENDED,
    H264PROFILE_MAIN,
    H264PROFILE_HIGH,
};

bool IsSupportedProfile(VideoCodecProfile profile) {
  for (const auto& supported_profile : kSupportedProfiles) {
    if (profile == supported_profile) {
      return true;
    }
  }
  return false;
}

}  // namespace

VideoToolboxVideoDecoder::VideoToolboxVideoDecoder(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    std::unique_ptr<MediaLog> media_log,
    scoped_refptr<base::SequencedTaskRunner> gpu_task_runner,
    GetCommandBufferStubCB get_stub_cb)
    : task_runner_(std::move(task_runner)),
      media_log_(std::move(media_log)),
      gpu_task_runner_(std::move(gpu_task_runner)),
      get_stub_cb_(std::move(get_stub_cb)),
      video_toolbox_(
          task_runner_,
          media_log_->Clone(),
          base::BindRepeating(&VideoToolboxVideoDecoder::OnVideoToolboxOutput,
                              base::Unretained(this)),
          base::BindRepeating(&VideoToolboxVideoDecoder::OnVideoToolboxError,
                              base::Unretained(this))),
      output_queue_(task_runner_) {
  DVLOG(1) << __func__;
}

VideoToolboxVideoDecoder::~VideoToolboxVideoDecoder() {
  DVLOG(1) << __func__;
}

bool VideoToolboxVideoDecoder::NeedsBitstreamConversion() const {
  DVLOG(4) << __func__;
  return true;
}

int VideoToolboxVideoDecoder::GetMaxDecodeRequests() const {
  DVLOG(4) << __func__;
  // This is kMaxVideoFrames, and it seems to have worked okay so far.
  return 4;
}

VideoDecoderType VideoToolboxVideoDecoder::GetDecoderType() const {
  DVLOG(4) << __func__;
  return VideoDecoderType::kVideoToolbox;
}

void VideoToolboxVideoDecoder::Initialize(const VideoDecoderConfig& config,
                                          bool low_delay,
                                          CdmContext* cdm_context,
                                          InitCB init_cb,
                                          const OutputCB& output_cb,
                                          const WaitingCB& waiting_cb) {
  DVLOG(1) << __func__;
  DCHECK(decode_cbs_.empty());
  DCHECK(config.IsValidConfig());

  if (has_error_) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(init_cb), DecoderStatus::Codes::kFailed));
    return;
  }

  if (!IsSupportedProfile(config.profile())) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(init_cb),
                                  DecoderStatus::Codes::kUnsupportedProfile));
    NotifyError(DecoderStatus::Codes::kUnsupportedProfile);
    return;
  }

  if (config.is_encrypted()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(init_cb),
                       DecoderStatus::Codes::kUnsupportedEncryptionMode));
    NotifyError(DecoderStatus::Codes::kUnsupportedEncryptionMode);
    return;
  }

  // If this is a reconfiguration, drop in-flight outputs.
  if (accelerator_) {
    ResetInternal(DecoderStatus::Codes::kAborted);
  }

  config_ = config;
  accelerator_ = std::make_unique<H264Decoder>(
      std::make_unique<VideoToolboxH264Accelerator>(
          media_log_->Clone(),
          base::BindRepeating(&VideoToolboxVideoDecoder::OnAcceleratorDecode,
                              base::Unretained(this)),
          base::BindRepeating(&VideoToolboxVideoDecoder::OnAcceleratorOutput,
                              base::Unretained(this))),
      config.profile(), config.color_space_info());
  output_queue_.SetOutputCB(output_cb);

  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(init_cb), DecoderStatus::Codes::kOk));
}

void VideoToolboxVideoDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                                      DecodeCB decode_cb) {
  DVLOG(3) << __func__;

  if (has_error_) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(decode_cb), DecoderStatus::Codes::kFailed));
    return;
  }

  // Flushes are handled differently from ordinary decodes.
  if (buffer->end_of_stream()) {
    if (!accelerator_->Flush()) {
      task_runner_->PostTask(
          FROM_HERE, base::BindOnce(std::move(decode_cb),
                                    DecoderStatus::Codes::kMalformedBitstream));
      NotifyError(DecoderStatus::Codes::kMalformedBitstream);
      return;
    }
    // Must be called after `accelerator_->Flush()` so that all outputs will
    // have been scheduled already.
    output_queue_.Flush(std::move(decode_cb));
    return;
  }

  decode_cbs_.push(std::move(decode_cb));
  accelerator_->SetStream(-1, *buffer);
  while (true) {
    // |active_decode_| is used in OnAcceleratorDecode() callbacks to look up
    // decode metadata.
    active_decode_ = buffer;
    AcceleratedVideoDecoder::DecodeResult result = accelerator_->Decode();
    active_decode_.reset();

    switch (result) {
      case AcceleratedVideoDecoder::kDecodeError:
      case AcceleratedVideoDecoder::kRanOutOfSurfaces:
      case AcceleratedVideoDecoder::kNeedContextUpdate:
      case AcceleratedVideoDecoder::kTryAgain:
        // More specific reasons are logged to the media log.
        NotifyError(DecoderStatus::Codes::kMalformedBitstream);
        return;

      case AcceleratedVideoDecoder::kConfigChange:
      case AcceleratedVideoDecoder::kColorSpaceChange:
        continue;

      case AcceleratedVideoDecoder::kRanOutOfStreamData:
        // The accelerator may not have produced any sample for decoding.
        ReleaseDecodeCallbacks();
        return;
    }
  }
}

void VideoToolboxVideoDecoder::Reset(base::OnceClosure reset_cb) {
  DVLOG(1) << __func__;

  if (has_error_) {
    task_runner_->PostTask(FROM_HERE, std::move(reset_cb));
    return;
  }

  ResetInternal(DecoderStatus::Codes::kAborted);
  task_runner_->PostTask(FROM_HERE, std::move(reset_cb));
}

void VideoToolboxVideoDecoder::NotifyError(DecoderStatus status) {
  DVLOG(1) << __func__;

  if (has_error_) {
    return;
  }

  has_error_ = true;
  ResetInternal(status);
}

void VideoToolboxVideoDecoder::ResetInternal(DecoderStatus status) {
  DVLOG(4) << __func__;

  while (!decode_cbs_.empty()) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(decode_cbs_.front()), status));
    decode_cbs_.pop();
  }

  accelerator_->Reset();
  video_toolbox_.Reset();
  output_queue_.Reset(status);

  // Drop in-flight conversions.
  converter_weak_this_factory_.InvalidateWeakPtrs();
}

void VideoToolboxVideoDecoder::ReleaseDecodeCallbacks() {
  DVLOG(4) << __func__;
  DCHECK(!has_error_);

  while (decode_cbs_.size() > video_toolbox_.NumDecodes()) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::move(decode_cbs_.front()),
                                          DecoderStatus::Codes::kOk));
    decode_cbs_.pop();
  }
}

void VideoToolboxVideoDecoder::OnAcceleratorDecode(
    base::ScopedCFTypeRef<CMSampleBufferRef> sample,
    scoped_refptr<CodecPicture> picture) {
  DVLOG(4) << __func__;
  DCHECK(active_decode_);

  auto metadata = std::make_unique<VideoToolboxDecodeMetadata>();
  metadata->picture = std::move(picture);
  metadata->timestamp = active_decode_->timestamp();
  metadata->duration = active_decode_->duration();
  metadata->aspect_ratio = config_.aspect_ratio();
  metadata->color_space = accelerator_->GetVideoColorSpace().ToGfxColorSpace();
  if (!metadata->color_space.IsValid()) {
    metadata->color_space = config_.color_space_info().ToGfxColorSpace();
  }
  metadata->hdr_metadata = accelerator_->GetHDRMetadata();
  if (!metadata->hdr_metadata) {
    metadata->hdr_metadata = config_.hdr_metadata();
  }

  video_toolbox_.Decode(std::move(sample), std::move(metadata));
}

void VideoToolboxVideoDecoder::OnAcceleratorOutput(
    scoped_refptr<CodecPicture> picture) {
  DVLOG(3) << __func__;
  output_queue_.SchedulePicture(std::move(picture));
}

void VideoToolboxVideoDecoder::OnVideoToolboxOutput(
    base::ScopedCFTypeRef<CVImageBufferRef> image,
    std::unique_ptr<VideoToolboxDecodeMetadata> metadata) {
  DVLOG(4) << __func__;

  if (has_error_) {
    return;
  }

  // Presumably there is at least one decode callback to release.
  ReleaseDecodeCallbacks();

  // Check if the frame was dropped.
  if (!image) {
    return;
  }

  // Lazily create `converter_`.
  if (!converter_) {
    converter_ = base::MakeRefCounted<VideoToolboxFrameConverter>(
        gpu_task_runner_, media_log_->Clone(), std::move(get_stub_cb_));
  }

  gpu_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &VideoToolboxFrameConverter::Convert, converter_, std::move(image),
          std::move(metadata),
          base::BindPostTask(
              task_runner_,
              base::BindOnce(&VideoToolboxVideoDecoder::OnConverterOutput,
                             converter_weak_this_factory_.GetWeakPtr()))));
}

void VideoToolboxVideoDecoder::OnVideoToolboxError(DecoderStatus status) {
  DVLOG(1) << __func__;
  NotifyError(std::move(status));
}

void VideoToolboxVideoDecoder::OnConverterOutput(
    scoped_refptr<VideoFrame> frame,
    std::unique_ptr<VideoToolboxDecodeMetadata> metadata) {
  DVLOG(4) << __func__;

  if (has_error_) {
    return;
  }

  if (!frame) {
    // More specific reasons are logged to the media log.
    NotifyError(DecoderStatus::Codes::kFailedToGetVideoFrame);
    return;
  }

  output_queue_.FulfillPicture(std::move(metadata->picture), std::move(frame));
}

}  // namespace media

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/mac/video_toolbox_video_decoder.h"

#include <VideoToolbox/VideoToolbox.h>

#include <memory>
#include <tuple>
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
#include "media/gpu/mac/video_toolbox_decompression_interface.h"
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
      get_stub_cb_(std::move(get_stub_cb)) {
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
  DCHECK(config.IsValidConfig());

  if (!has_error_) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(init_cb), DecoderStatus::Codes::kFailed));
    return;
  }

  // Make |init_cb| available to NotifyError().
  init_cb_ = std::move(init_cb);

  if (!IsSupportedProfile(config.profile())) {
    NotifyError(DecoderStatus::Codes::kUnsupportedProfile);
    return;
  }

  if (config.is_encrypted()) {
    NotifyError(DecoderStatus::Codes::kUnsupportedEncryptionMode);
    return;
  }

  if (!accelerator_) {
    accelerator_ = std::make_unique<H264Decoder>(
        std::make_unique<VideoToolboxH264Accelerator>(
            media_log_->Clone(),
            base::BindRepeating(&VideoToolboxVideoDecoder::OnAcceleratorDecode,
                                base::Unretained(this)),
            base::BindRepeating(&VideoToolboxVideoDecoder::OnAcceleratorOutput,
                                base::Unretained(this))),
        config.profile(), config.color_space_info());

    video_toolbox_ = std::make_unique<VideoToolboxDecompressionInterface>(
        task_runner_, media_log_->Clone(),
        base::BindRepeating(&VideoToolboxVideoDecoder::OnVideoToolboxOutput,
                            base::Unretained(this)),
        base::BindRepeating(&VideoToolboxVideoDecoder::OnVideoToolboxError,
                            base::Unretained(this)));

    converter_ = base::MakeRefCounted<VideoToolboxFrameConverter>(
        gpu_task_runner_, media_log_->Clone(), std::move(get_stub_cb_));
  } else {
    // TODO(crbug.com/1331597): Support codec changes.
    // TODO(crbug.com/1331597): Handle color space changes.
    if (config.codec() != config_.codec()) {
      NotifyError(DecoderStatus::Codes::kCantChangeCodec);
      return;
    }
  }

  config_ = config;
  output_cb_ = output_cb;

  task_runner_->PostTask(FROM_HERE, base::BindOnce(std::move(init_cb_),
                                                   DecoderStatus::Codes::kOk));
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

  if (buffer->end_of_stream()) {
    flush_cb_ = std::move(decode_cb);
    if (!accelerator_->Flush()) {
      NotifyError(DecoderStatus::Codes::kMalformedBitstream);
      return;
    }
    ProcessOutputs();
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
        // If decoding did not produce any sample, a decode callback should be
        // released immediately.
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

  if (init_cb_) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::move(init_cb_), status));
  }

  while (!decode_cbs_.empty()) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(decode_cbs_.front()), status));
    decode_cbs_.pop();
  }

  if (flush_cb_) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::move(flush_cb_), status));
  }

  accelerator_->Reset();
  video_toolbox_->Reset();

  decode_metadata_.clear();
  output_queue_ = {};
  output_frames_.clear();

  // Drop in-flight frame conversions.
  converter_weak_this_factory_.InvalidateWeakPtrs();
}

void VideoToolboxVideoDecoder::ReleaseDecodeCallbacks() {
  DVLOG(4) << __func__;
  DCHECK(!has_error_);

  while (decode_cbs_.size() > video_toolbox_->PendingDecodes()) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::move(decode_cbs_.front()),
                                          DecoderStatus::Codes::kOk));
    decode_cbs_.pop();
  }
}

void VideoToolboxVideoDecoder::ProcessOutputs() {
  DVLOG(4) << __func__;
  DCHECK(!has_error_);

  while (!output_queue_.empty()) {
    void* context = static_cast<void*>(output_queue_.front().get());
    if (!output_frames_.contains(context)) {
      // The frame has not been decoded or converted yet.
      break;
    }

    DVLOG(4) << __func__ << ": Output " << output_frames_[context]->timestamp();
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(output_cb_, std::move(output_frames_[context])));

    output_frames_.erase(context);
    output_queue_.pop();
  }

  // If there is an active flush and no more outputs, complete the flush.
  if (flush_cb_ && output_queue_.empty()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(flush_cb_), DecoderStatus::Codes::kOk));
  }
}

void VideoToolboxVideoDecoder::OnAcceleratorDecode(
    base::ScopedCFTypeRef<CMSampleBufferRef> sample,
    scoped_refptr<CodecPicture> picture) {
  DVLOG(4) << __func__;
  void* context = static_cast<void*>(picture.get());
  decode_metadata_[context] = DecodeMetadata{active_decode_->timestamp()};
  video_toolbox_->Decode(std::move(sample), context);
}

void VideoToolboxVideoDecoder::OnAcceleratorOutput(
    scoped_refptr<CodecPicture> picture) {
  DVLOG(3) << __func__;
  output_queue_.push(std::move(picture));
  ProcessOutputs();
}

void VideoToolboxVideoDecoder::OnVideoToolboxOutput(
    base::ScopedCFTypeRef<CVImageBufferRef> image,
    void* context) {
  DVLOG(4) << __func__;

  if (has_error_) {
    return;
  }

  gpu_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &VideoToolboxFrameConverter::Convert, converter_, std::move(image),
          decode_metadata_[context].timestamp, context,
          base::BindPostTask(
              task_runner_,
              base::BindOnce(&VideoToolboxVideoDecoder::OnConverterOutput,
                             converter_weak_this_factory_.GetWeakPtr()))));

  // All the metadata was passed to Convert(), we don't need it anymore.
  decode_metadata_.erase(context);

  // Presumably there is at least one decode callback to release.
  ReleaseDecodeCallbacks();
}

void VideoToolboxVideoDecoder::OnVideoToolboxError(DecoderStatus status) {
  DVLOG(1) << __func__;
  NotifyError(std::move(status));
}

void VideoToolboxVideoDecoder::OnConverterOutput(
    scoped_refptr<VideoFrame> frame,
    void* context) {
  DVLOG(4) << __func__;

  if (has_error_) {
    return;
  }

  if (!frame) {
    // More specific reasons are logged to the media log.
    NotifyError(DecoderStatus::Codes::kFailedToGetVideoFrame);
    return;
  }

  output_frames_[context] = std::move(frame);

  ProcessOutputs();
}

}  // namespace media

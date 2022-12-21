// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/video_encoder_fallback.h"

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "media/base/video_frame.h"
#include "media/video/video_encoder_info.h"

namespace media {

VideoEncoderFallback::VideoEncoderFallback(
    std::unique_ptr<VideoEncoder> main_encoder,
    CreateFallbackCB create_fallback_cb)
    : encoder_(std::move(main_encoder)),
      create_fallback_cb_(std::move(create_fallback_cb)) {
  DCHECK(encoder_);
  DCHECK(!create_fallback_cb_.is_null());
}

VideoEncoderFallback::~VideoEncoderFallback() = default;

void VideoEncoderFallback::Initialize(VideoCodecProfile profile,
                                      const Options& options,
                                      EncoderInfoCB info_cb,
                                      OutputCB output_cb,
                                      EncoderStatusCB done_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  init_done_cb_ = std::move(done_cb);
  info_cb_ = std::move(info_cb);
  output_cb_ = std::move(output_cb);
  profile_ = profile;
  options_ = options;
  auto done_callback = [](base::WeakPtr<VideoEncoderFallback> self,
                          EncoderStatus status) {
    if (!self)
      return;
    if (status.is_ok()) {
      std::move(self->init_done_cb_).Run(std::move(status));
      return;
    }
    self->FallbackInitialize();
  };

  encoder_->Initialize(
      profile, options,
      base::BindRepeating(&VideoEncoderFallback::CallInfo,
                          weak_factory_.GetWeakPtr()),
      base::BindRepeating(&VideoEncoderFallback::CallOutput,
                          weak_factory_.GetWeakPtr()),
      base::BindOnce(done_callback, weak_factory_.GetWeakPtr()));
}

VideoEncoder::PendingEncode VideoEncoderFallback::MakePendingEncode(
    scoped_refptr<VideoFrame> frame,
    bool key_frame,
    EncoderStatusCB done_cb) {
  PendingEncode result;
  result.done_callback = std::move(done_cb);
  result.frame = std::move(frame);
  result.key_frame = key_frame;
  return result;
}

void VideoEncoderFallback::Encode(scoped_refptr<VideoFrame> frame,
                                  bool key_frame,
                                  EncoderStatusCB done_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!init_done_cb_);

  if (use_fallback_) {
    if (fallback_initialized_) {
      encoder_->Encode(std::move(frame), key_frame, std::move(done_cb));
    } else {
      encodes_to_retry_.push_back(std::make_unique<PendingEncode>(
          MakePendingEncode(std::move(frame), key_frame, std::move(done_cb))));
    }
    return;
  }

  auto done_callback = [](base::WeakPtr<VideoEncoderFallback> self,
                          PendingEncode args, EncoderStatus status) {
    if (!self)
      return;
    DCHECK(self->encoder_);
    if (status.is_ok()) {
      std::move(args.done_callback).Run(std::move(status));
      return;
    }
    self->FallbackEncode(std::move(args));
  };

  encoder_->Encode(
      frame, key_frame,
      base::BindOnce(done_callback, weak_factory_.GetWeakPtr(),
                     MakePendingEncode(frame, key_frame, std::move(done_cb))));
}

void VideoEncoderFallback::ChangeOptions(const Options& options,
                                         OutputCB output_cb,
                                         EncoderStatusCB done_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  options_ = options;
  if (encoder_)
    encoder_->ChangeOptions(options, std::move(output_cb), std::move(done_cb));
}

void VideoEncoderFallback::Flush(EncoderStatusCB done_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (encoder_)
    encoder_->Flush(std::move(done_cb));
}

void VideoEncoderFallback::FallbackInitCompleted(EncoderStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(encoder_);
  if (init_done_cb_)
    std::move(init_done_cb_).Run(status);
  fallback_initialized_ = true;

  if (status.is_ok()) {
    for (auto& encode : encodes_to_retry_) {
      encoder_->Encode(std::move(encode->frame), encode->key_frame,
                       std::move(encode->done_callback));
    }
  } else {
    for (auto& encode : encodes_to_retry_)
      std::move(encode->done_callback).Run(status);
  }
  encodes_to_retry_.clear();
}

void VideoEncoderFallback::FallbackInitialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  use_fallback_ = true;
  encoder_ = std::move(create_fallback_cb_).Run();
  if (!encoder_) {
    std::move(init_done_cb_)
        .Run(EncoderStatus::Codes::kEncoderInitializationError);
    FallbackInitCompleted(EncoderStatus::Codes::kEncoderInitializationError);
    return;
  }

  encoder_->Initialize(
      profile_, options_,
      base::BindRepeating(&VideoEncoderFallback::CallInfo,
                          weak_factory_.GetWeakPtr()),
      base::BindRepeating(&VideoEncoderFallback::CallOutput,
                          weak_factory_.GetWeakPtr()),
      base::BindOnce(&VideoEncoderFallback::FallbackInitCompleted,
                     weak_factory_.GetWeakPtr()));
}

void VideoEncoderFallback::FallbackEncode(PendingEncode args) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!use_fallback_) {
    use_fallback_ = true;
    encoder_ = std::move(create_fallback_cb_).Run();
    if (!encoder_) {
      std::move(args.done_callback)
          .Run(EncoderStatus::Codes::kEncoderInitializationError);
      return;
    }

    encoder_->Initialize(
        profile_, options_,
        base::BindRepeating(&VideoEncoderFallback::CallInfo,
                            weak_factory_.GetWeakPtr()),
        base::BindRepeating(&VideoEncoderFallback::CallOutput,
                            weak_factory_.GetWeakPtr()),
        base::BindOnce(&VideoEncoderFallback::FallbackInitCompleted,
                       weak_factory_.GetWeakPtr()));
  }

  if (fallback_initialized_) {
    encoder_->Encode(std::move(args.frame), args.key_frame,
                     std::move(args.done_callback));
  } else {
    encodes_to_retry_.push_back(
        std::make_unique<PendingEncode>(std::move(args)));
  }
}

void VideoEncoderFallback::CallInfo(const VideoEncoderInfo& encoder_info) {
  info_cb_.Run(encoder_info);
}

void VideoEncoderFallback::CallOutput(VideoEncoderOutput output,
                                      absl::optional<CodecDescription> desc) {
  output_cb_.Run(std::move(output), std::move(desc));
}

}  // namespace media

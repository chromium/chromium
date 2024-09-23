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

  info_cb_ = std::move(info_cb);
  output_cb_ = std::move(output_cb);
  profile_ = profile;
  options_ = options;
  auto done_callback = [](base::WeakPtr<VideoEncoderFallback> self,
                          EncoderStatusCB init_done_cb, EncoderStatus status) {
    if (!self)
      return;
    if (status.is_ok()) {
      std::move(init_done_cb).Run(std::move(status));
      return;
    }
    self->FallbackInitialize(std::move(init_done_cb));
  };

  encoder_->Initialize(profile, options,
                       base::BindRepeating(&VideoEncoderFallback::CallInfo,
                                           weak_factory_.GetWeakPtr()),
                       base::BindRepeating(&VideoEncoderFallback::CallOutput,
                                           weak_factory_.GetWeakPtr()),
                       base::BindOnce(done_callback, weak_factory_.GetWeakPtr(),
                                      std::move(done_cb)));
}

VideoEncoder::PendingEncode VideoEncoderFallback::MakePendingEncode(
    scoped_refptr<VideoFrame> frame,
    const EncodeOptions& encode_options,
    EncoderStatusCB done_cb) {
  PendingEncode result;
  result.done_callback = std::move(done_cb);
  result.frame = std::move(frame);
  result.options = encode_options;
  return result;
}

void VideoEncoderFallback::Encode(scoped_refptr<VideoFrame> frame,
                                  const EncodeOptions& encode_options,
                                  EncoderStatusCB done_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (state_ == State::kError) {
    std::move(done_cb).Run(EncoderStatus::Codes::kEncoderFailedEncode);
    return;
  }

  if (state_ == State::kInitializingFallbackEncoder) {
    encodes_to_retry_.push_back(
        std::make_unique<PendingEncode>(MakePendingEncode(
            std::move(frame), encode_options, std::move(done_cb))));
    return;
  }

  if (state_ == State::kMainEncoder) {
    auto done_callback = [](base::WeakPtr<VideoEncoderFallback> self,
                            PendingEncode args, EncoderStatus status) {
      if (!self) {
        return;
      }
      if (self->state_ == State::kError) {
        // This happens when the fallback happens for a prior frame and the
        // fallback encoder creation or initialization fails.
        // TODO(b/339759433): We should rather ignore this case because this
        // must be handled during the fallback process.
        std::move(args.done_callback)
            .Run(EncoderStatus::Codes::kEncoderFailedEncode);
        return;
      }
      if (status.is_ok()) {
        std::move(args.done_callback).Run(std::move(status));
        return;
      }
      self->FallbackEncode(std::move(args), std::move(status));
    };
    done_cb = base::BindOnce(
        done_callback, weak_factory_.GetWeakPtr(),
        MakePendingEncode(frame, encode_options, std::move(done_cb)));
  }
  encoder_->Encode(std::move(frame), encode_options, std::move(done_cb));
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

void VideoEncoderFallback::FallbackInitCompleted(PendingEncode args,
                                                 EncoderStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(encoder_);
  state_ = State::kFallbackEncoder;

  encodes_to_retry_.insert(encodes_to_retry_.begin(),
                           std::make_unique<PendingEncode>(std::move(args)));
  if (status.is_ok()) {
    for (auto& encode : encodes_to_retry_) {
      encoder_->Encode(std::move(encode->frame), encode->options,
                       std::move(encode->done_callback));
    }
  } else {
    state_ = State::kError;
    for (auto& encode : encodes_to_retry_)
      std::move(encode->done_callback).Run(status);
  }
  encodes_to_retry_.clear();
}

void VideoEncoderFallback::FallbackInitialize(EncoderStatusCB init_done_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, State::kMainEncoder);
  encoder_.reset();
  auto fallback_encoder = std::move(create_fallback_cb_).Run();
  if (!fallback_encoder.has_value()) {
    state_ = State::kError;
    std::move(init_done_cb)
        .Run(EncoderStatus::Codes::kEncoderInitializationError);
    return;
  }
  state_ = State::kInitializingFallbackEncoder;
  encoder_ = std::move(fallback_encoder).value();

  auto complete_cb = [](base::WeakPtr<VideoEncoderFallback> self,
                        EncoderStatusCB init_done_cb, EncoderStatus status) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(self->sequence_checker_);
    DCHECK(self->encoder_);
    std::move(init_done_cb).Run(std::move(status));
    self->state_ = State::kFallbackEncoder;
  };

  // Initializes fallback encoder from main encoder's initialization failure.
  encoder_->Initialize(profile_, options_,
                       base::BindRepeating(&VideoEncoderFallback::CallInfo,
                                           weak_factory_.GetWeakPtr()),
                       base::BindRepeating(&VideoEncoderFallback::CallOutput,
                                           weak_factory_.GetWeakPtr()),
                       base::BindOnce(complete_cb, weak_factory_.GetWeakPtr(),
                                      std::move(init_done_cb)));
  return;
}

void VideoEncoderFallback::FallbackEncode(PendingEncode args,
                                          EncoderStatus main_encoder_status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The first frame on which the error status is returned with the main
  // encoder's Encode().
  if (state_ == State::kMainEncoder) {
    encoder_.reset();
    auto fallback_encoder = std::move(create_fallback_cb_).Run();
    if (!fallback_encoder.has_value()) {
      state_ = State::kError;
      std::move(args.done_callback).Run(main_encoder_status.code());
      return;
    }
    state_ = State::kInitializingFallbackEncoder;
    encoder_ = std::move(fallback_encoder).value();
    encoder_->Initialize(
        profile_, options_,
        base::BindRepeating(&VideoEncoderFallback::CallInfo,
                            weak_factory_.GetWeakPtr()),
        base::BindRepeating(&VideoEncoderFallback::CallOutput,
                            weak_factory_.GetWeakPtr()),
        base::BindOnce(&VideoEncoderFallback::FallbackInitCompleted,
                       weak_factory_.GetWeakPtr(), std::move(args)));
    return;
  }
  DCHECK_EQ(state_, State::kInitializingFallbackEncoder);
  encodes_to_retry_.push_back(std::make_unique<PendingEncode>(std::move(args)));
}

void VideoEncoderFallback::CallInfo(const VideoEncoderInfo& encoder_info) {
  if (info_cb_) {
    info_cb_.Run(encoder_info);
  }
}

void VideoEncoderFallback::CallOutput(VideoEncoderOutput output,
                                      std::optional<CodecDescription> desc) {
  output_cb_.Run(std::move(output), std::move(desc));
}

}  // namespace media

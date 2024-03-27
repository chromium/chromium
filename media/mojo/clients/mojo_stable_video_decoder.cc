// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/clients/mojo_stable_video_decoder.h"

#include "media/gpu/chromeos/oop_video_decoder.h"

namespace media {

MojoStableVideoDecoder::MojoStableVideoDecoder(
    scoped_refptr<base::SequencedTaskRunner> media_task_runner,
    MediaLog* media_log,
    mojo::PendingRemote<stable::mojom::StableVideoDecoder>
        pending_remote_decoder)
    : media_task_runner_(std::move(media_task_runner)),
      media_log_(media_log),
      pending_remote_decoder_(std::move(pending_remote_decoder)),
      weak_this_factory_(this) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

MojoStableVideoDecoder::~MojoStableVideoDecoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool MojoStableVideoDecoder::IsPlatformDecoder() const {
  return true;
}

bool MojoStableVideoDecoder::SupportsDecryption() const {
  // TODO(b/327268445): implement decoding of protected content for GTFO OOP-VD.
  return false;
}

void MojoStableVideoDecoder::Initialize(const VideoDecoderConfig& config,
                                        bool low_delay,
                                        CdmContext* cdm_context,
                                        InitCB init_cb,
                                        const OutputCB& output_cb,
                                        const WaitingCB& waiting_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  OOPVideoDecoder::NotifySupportKnown(
      std::move(pending_remote_decoder_),
      base::BindOnce(
          &MojoStableVideoDecoder::InitializeOnceSupportedConfigsAreKnown,
          weak_this_factory_.GetWeakPtr()));
}

void MojoStableVideoDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                                    DecodeCB decode_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(b/327268445): finish implementing Decode().
  NOTIMPLEMENTED();
}

void MojoStableVideoDecoder::Reset(base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(b/327268445): finish implementing Reset().
  NOTIMPLEMENTED();
}

bool MojoStableVideoDecoder::NeedsBitstreamConversion() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(b/327268445): finish implementing NeedsBitstreamConversion().
  NOTIMPLEMENTED();
  return false;
}

bool MojoStableVideoDecoder::CanReadWithoutStalling() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(b/327268445): finish implementing CanReadWithoutStalling().
  NOTIMPLEMENTED();
  return false;
}

int MojoStableVideoDecoder::GetMaxDecodeRequests() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(b/327268445): finish implementing GetMaxDecodeRequests().
  NOTIMPLEMENTED();
  return 8;
}

VideoDecoderType MojoStableVideoDecoder::GetDecoderType() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(b/327268445): finish implementing GetDecoderType().
  NOTIMPLEMENTED();
  return VideoDecoderType::kOutOfProcess;
}

void MojoStableVideoDecoder::InitializeOnceSupportedConfigsAreKnown(
    mojo::PendingRemote<stable::mojom::StableVideoDecoder>
        pending_remote_decoder) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!oop_video_decoder_) {
    // This should correspond to the first MojoStableVideoDecoder::Initialize()
    // call, so |pending_remote_decoder| and |media_log_| must be valid.
    CHECK(pending_remote_decoder);
    CHECK(media_log_);
    oop_video_decoder_ = OOPVideoDecoder::Create(
        std::move(pending_remote_decoder), media_log_->Clone(),
        /*decoder_task_runner=*/media_task_runner_,
        /*client=*/nullptr);
    CHECK(oop_video_decoder_);
    media_log_ = nullptr;
  }

  // TODO(b/327268445): finish the initialization of the |oop_video_decoder_|.
  NOTIMPLEMENTED();
}

}  // namespace media

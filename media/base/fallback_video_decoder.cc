// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "media/base/decoder_buffer.h"
#include "media/base/fallback_video_decoder.h"
#include "media/base/video_decoder_config.h"

namespace media {

FallbackVideoDecoder::FallbackVideoDecoder(
    std::unique_ptr<VideoDecoder> preferred,
    std::unique_ptr<VideoDecoder> fallback)
    : preferred_decoder_(std::move(preferred)),
      fallback_decoder_(std::move(fallback)),
      weak_factory_(this) {}

void FallbackVideoDecoder::Initialize(
    const VideoDecoderConfig& config,
    bool low_delay,
    CdmContext* cdm_context,
    const InitCB& init_cb,
    const OutputCB& output_cb,
    const WaitingForDecryptionKeyCB& waiting_for_decryption_key_cb) {
  // If we've already fallen back, just reinitialize the selected decoder.
  if (selected_decoder_ && did_fallback_) {
    selected_decoder_->Initialize(config, low_delay, cdm_context, init_cb,
                                  output_cb, waiting_for_decryption_key_cb);
    return;
  }

  InitCB fallback_initialize_cb = base::BindRepeating(
      &FallbackVideoDecoder::FallbackInitialize, weak_factory_.GetWeakPtr(),
      config, low_delay, cdm_context, init_cb, output_cb,
      waiting_for_decryption_key_cb);

  preferred_decoder_->Initialize(config, low_delay, cdm_context,
                                 std::move(fallback_initialize_cb), output_cb,
                                 waiting_for_decryption_key_cb);
}

void FallbackVideoDecoder::FallbackInitialize(
    const VideoDecoderConfig& config,
    bool low_delay,
    CdmContext* cdm_context,
    const InitCB& init_cb,
    const OutputCB& output_cb,
    const WaitingForDecryptionKeyCB& waiting_for_decryption_key_cb,
    bool success) {
  // The preferred decoder was successfully initialized.
  if (success) {
    selected_decoder_ = preferred_decoder_.get();
    init_cb.Run(true);
    return;
  }

  did_fallback_ = true;
  // Post destruction of |preferred_decoder_| so that we don't destroy the
  // object during the callback.  DeleteSoon doesn't handle custom deleters, so
  // we post a do-nothing task instead.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(base::DoNothing::Once<std::unique_ptr<VideoDecoder>>(),
                     std::move(preferred_decoder_)));
  selected_decoder_ = fallback_decoder_.get();
  fallback_decoder_->Initialize(config, low_delay, cdm_context, init_cb,
                                output_cb, waiting_for_decryption_key_cb);
}

void FallbackVideoDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                                  const DecodeCB& decode_cb) {
  DCHECK(selected_decoder_);
  selected_decoder_->Decode(std::move(buffer), decode_cb);
}

void FallbackVideoDecoder::Reset(const base::RepeatingClosure& reset_cb) {
  DCHECK(selected_decoder_);
  selected_decoder_->Reset(reset_cb);
}

bool FallbackVideoDecoder::NeedsBitstreamConversion() const {
  DCHECK(selected_decoder_);
  return selected_decoder_->NeedsBitstreamConversion();
}

bool FallbackVideoDecoder::CanReadWithoutStalling() const {
  DCHECK(selected_decoder_);
  return selected_decoder_->CanReadWithoutStalling();
}

int FallbackVideoDecoder::GetMaxDecodeRequests() const {
  DCHECK(selected_decoder_);
  return selected_decoder_->GetMaxDecodeRequests();
}

std::string FallbackVideoDecoder::GetDisplayName() const {
  // MojoVideoDecoder always identifies itself as such, and never asks for the
  // name of the underlying decoder.
  NOTREACHED();
  return "FallbackVideoDecoder";
}

FallbackVideoDecoder::~FallbackVideoDecoder() = default;

}  // namespace media

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/fuchsia/cdm/fuchsia_decryptor.h"

#include "base/fuchsia/fuchsia_logging.h"
#include "base/logging.h"
#include "media/base/decoder_buffer.h"
#include "media/base/video_frame.h"
#include "media/fuchsia/cdm/fuchsia_stream_decryptor.h"

namespace media {

FuchsiaDecryptor::FuchsiaDecryptor(
    fuchsia::media::drm::ContentDecryptionModule* cdm)
    : cdm_(cdm) {
  DCHECK(cdm_);
}

FuchsiaDecryptor::~FuchsiaDecryptor() = default;

void FuchsiaDecryptor::RegisterNewKeyCB(StreamType stream_type,
                                        const NewKeyCB& new_key_cb) {
  if (stream_type != kAudio)
    return;

  base::AutoLock auto_lock(new_key_cb_lock_);
  new_key_cb_ = new_key_cb;
}

void FuchsiaDecryptor::Decrypt(StreamType stream_type,
                               scoped_refptr<DecoderBuffer> encrypted,
                               const DecryptCB& decrypt_cb) {
  if (stream_type != StreamType::kAudio) {
    decrypt_cb.Run(Status::kError, nullptr);
    return;
  }

  if (!audio_decryptor_)
    audio_decryptor_ = FuchsiaClearStreamDecryptor::Create(cdm_);

  audio_decryptor_->Decrypt(std::move(encrypted), decrypt_cb);
}

void FuchsiaDecryptor::CancelDecrypt(StreamType stream_type) {
  if (stream_type == StreamType::kAudio && audio_decryptor_) {
    audio_decryptor_->CancelDecrypt();
  }
}

void FuchsiaDecryptor::InitializeAudioDecoder(const AudioDecoderConfig& config,
                                              const DecoderInitCB& init_cb) {
  // Only decryption is supported.
  init_cb.Run(false);
}

void FuchsiaDecryptor::InitializeVideoDecoder(const VideoDecoderConfig& config,
                                              const DecoderInitCB& init_cb) {
  // Only decryption is supported.
  init_cb.Run(false);
}

void FuchsiaDecryptor::DecryptAndDecodeAudio(
    scoped_refptr<DecoderBuffer> encrypted,
    const AudioDecodeCB& audio_decode_cb) {
  NOTREACHED();
  audio_decode_cb.Run(Status::kError, AudioFrames());
}

void FuchsiaDecryptor::DecryptAndDecodeVideo(
    scoped_refptr<DecoderBuffer> encrypted,
    const VideoDecodeCB& video_decode_cb) {
  NOTREACHED();
  video_decode_cb.Run(Status::kError, nullptr);
}

void FuchsiaDecryptor::ResetDecoder(StreamType stream_type) {
  NOTREACHED();
}

void FuchsiaDecryptor::DeinitializeDecoder(StreamType stream_type) {
  NOTREACHED();
}

bool FuchsiaDecryptor::CanAlwaysDecrypt() {
  return false;
}

void FuchsiaDecryptor::OnNewKey() {
  base::AutoLock auto_lock(new_key_cb_lock_);
  if (new_key_cb_)
    new_key_cb_.Run();
}

}  // namespace media

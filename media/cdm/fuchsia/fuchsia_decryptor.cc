// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/fuchsia/fuchsia_decryptor.h"

#include "base/check.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/location.h"
#include "base/notreached.h"
#include "media/base/decoder_buffer.h"
#include "media/base/video_frame.h"
#include "media/cdm/fuchsia/fuchsia_cdm_context.h"
#include "media/cdm/fuchsia/fuchsia_stream_decryptor.h"

namespace media {

FuchsiaDecryptor::FuchsiaDecryptor(FuchsiaCdmContext* cdm_context)
    : cdm_context_(cdm_context) {
  DCHECK(cdm_context_);
}

FuchsiaDecryptor::~FuchsiaDecryptor() {}

void FuchsiaDecryptor::Decrypt(StreamType stream_type,
                               scoped_refptr<DecoderBuffer> encrypted,
                               DecryptCB decrypt_cb) {
  std::move(decrypt_cb).Run(Status::kError, nullptr);
}

void FuchsiaDecryptor::CancelDecrypt(StreamType stream_type) {
  NOTREACHED_IN_MIGRATION();
}

void FuchsiaDecryptor::InitializeAudioDecoder(const AudioDecoderConfig& config,
                                              DecoderInitCB init_cb) {
  // Only decryption is supported.
  std::move(init_cb).Run(false);
}

void FuchsiaDecryptor::InitializeVideoDecoder(const VideoDecoderConfig& config,
                                              DecoderInitCB init_cb) {
  // Only decryption is supported.
  std::move(init_cb).Run(false);
}

void FuchsiaDecryptor::DecryptAndDecodeAudio(
    scoped_refptr<DecoderBuffer> encrypted,
    AudioDecodeCB audio_decode_cb) {
  NOTREACHED_IN_MIGRATION();
  std::move(audio_decode_cb).Run(Status::kError, AudioFrames());
}

void FuchsiaDecryptor::DecryptAndDecodeVideo(
    scoped_refptr<DecoderBuffer> encrypted,
    VideoDecodeCB video_decode_cb) {
  NOTREACHED_IN_MIGRATION();
  std::move(video_decode_cb).Run(Status::kError, nullptr);
}

void FuchsiaDecryptor::ResetDecoder(StreamType stream_type) {
  NOTREACHED_IN_MIGRATION();
}

void FuchsiaDecryptor::DeinitializeDecoder(StreamType stream_type) {
  NOTREACHED_IN_MIGRATION();
}

bool FuchsiaDecryptor::CanAlwaysDecrypt() {
  return false;
}

}  // namespace media

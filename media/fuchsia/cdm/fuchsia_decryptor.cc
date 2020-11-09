// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/fuchsia/cdm/fuchsia_decryptor.h"

#include "base/check.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/base/decoder_buffer.h"
#include "media/base/video_frame.h"
#include "media/fuchsia/cdm/fuchsia_stream_decryptor.h"

namespace media {

// Audio packets are normally smaller than 128kB (more than enough for 2 seconds
// at 320kb/s).
const size_t kAudioStreamBufferSize = 128 * 1024;

FuchsiaDecryptor::FuchsiaDecryptor(
    fuchsia::media::drm::ContentDecryptionModule* cdm)
    : cdm_(cdm) {
  DCHECK(cdm_);
}

FuchsiaDecryptor::~FuchsiaDecryptor() {
  if (audio_decryptor_) {
    audio_decryptor_task_runner_->DeleteSoon(FROM_HERE,
                                             std::move(audio_decryptor_));
  }
}

void FuchsiaDecryptor::Decrypt(StreamType stream_type,
                               scoped_refptr<DecoderBuffer> encrypted,
                               DecryptCB decrypt_cb) {
  if (stream_type != StreamType::kAudio) {
    std::move(decrypt_cb).Run(Status::kError, nullptr);
    return;
  }

  if (!audio_decryptor_) {
    audio_decryptor_task_runner_ = base::ThreadTaskRunnerHandle::Get();
    audio_decryptor_ =
        FuchsiaClearStreamDecryptor::Create(cdm_, kAudioStreamBufferSize);
  }

  audio_decryptor_->Decrypt(std::move(encrypted), std::move(decrypt_cb));
}

void FuchsiaDecryptor::CancelDecrypt(StreamType stream_type) {
  if (stream_type == StreamType::kAudio && audio_decryptor_) {
    audio_decryptor_->CancelDecrypt();
  }
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

}  // namespace media

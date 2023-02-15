// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_FUCHSIA_FUCHSIA_DECRYPTOR_H_
#define MEDIA_CDM_FUCHSIA_FUCHSIA_DECRYPTOR_H_

#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "base/thread_annotations.h"
#include "media/base/decryptor.h"
#include "media/cdm/fuchsia/fuchsia_stream_decryptor.h"

namespace media {

class FuchsiaCdmContext;

class FuchsiaDecryptor : public Decryptor {
 public:
  // Caller should make sure |cdm| lives longer than this class.
  explicit FuchsiaDecryptor(FuchsiaCdmContext* cdm_context);

  FuchsiaDecryptor(const FuchsiaDecryptor&) = delete;
  FuchsiaDecryptor& operator=(const FuchsiaDecryptor&) = delete;

  ~FuchsiaDecryptor() override;

  // media::Decryptor implementation:
  void Decrypt(StreamType stream_type,
               scoped_refptr<DecoderBuffer> encrypted,
               DecryptCB decrypt_cb) override;
  void CancelDecrypt(StreamType stream_type) override;
  void InitializeAudioDecoder(const AudioDecoderConfig& config,
                              DecoderInitCB init_cb) override;
  void InitializeVideoDecoder(const VideoDecoderConfig& config,
                              DecoderInitCB init_cb) override;
  void DecryptAndDecodeAudio(scoped_refptr<DecoderBuffer> encrypted,
                             AudioDecodeCB audio_decode_cb) override;
  void DecryptAndDecodeVideo(scoped_refptr<DecoderBuffer> encrypted,
                             VideoDecodeCB video_decode_cb) override;
  void ResetDecoder(StreamType stream_type) override;
  void DeinitializeDecoder(StreamType stream_type) override;
  bool CanAlwaysDecrypt() override;

 private:
  FuchsiaCdmContext* const cdm_context_;

  // TaskRunner for the thread on which |audio_decryptor_| was created.
  scoped_refptr<base::SingleThreadTaskRunner> audio_decryptor_task_runner_;
};

}  // namespace media

#endif  // MEDIA_CDM_FUCHSIA_FUCHSIA_DECRYPTOR_H_

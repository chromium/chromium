// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FUCHSIA_CDM_FUCHSIA_DECRYPTOR_H_
#define MEDIA_FUCHSIA_CDM_FUCHSIA_DECRYPTOR_H_

#include <memory>

#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "media/base/decryptor.h"
#include "media/fuchsia/cdm/fuchsia_stream_decryptor.h"

namespace fuchsia {
namespace media {
namespace drm {
class ContentDecryptionModule;
}  // namespace drm
}  // namespace media
}  // namespace fuchsia

namespace media {

class FuchsiaClearStreamDecryptor;

class FuchsiaDecryptor : public Decryptor {
 public:
  // Caller should make sure |cdm| lives longer than this class.
  explicit FuchsiaDecryptor(fuchsia::media::drm::ContentDecryptionModule* cdm);
  ~FuchsiaDecryptor() override;

  // media::Decryptor implementation:
  void RegisterNewKeyCB(StreamType stream_type,
                        const NewKeyCB& key_added_cb) override;
  void Decrypt(StreamType stream_type,
               scoped_refptr<DecoderBuffer> encrypted,
               const DecryptCB& decrypt_cb) override;
  void CancelDecrypt(StreamType stream_type) override;
  void InitializeAudioDecoder(const AudioDecoderConfig& config,
                              const DecoderInitCB& init_cb) override;
  void InitializeVideoDecoder(const VideoDecoderConfig& config,
                              const DecoderInitCB& init_cb) override;
  void DecryptAndDecodeAudio(scoped_refptr<DecoderBuffer> encrypted,
                             const AudioDecodeCB& audio_decode_cb) override;
  void DecryptAndDecodeVideo(scoped_refptr<DecoderBuffer> encrypted,
                             const VideoDecodeCB& video_decode_cb) override;
  void ResetDecoder(StreamType stream_type) override;
  void DeinitializeDecoder(StreamType stream_type) override;
  bool CanAlwaysDecrypt() override;

  // Called by FuchsiaCdm to notify about the new key.
  void OnNewKey();

 private:
  fuchsia::media::drm::ContentDecryptionModule* const cdm_;

  base::Lock new_key_cb_lock_;
  NewKeyCB new_key_cb_ GUARDED_BY(new_key_cb_lock_);

  std::unique_ptr<FuchsiaClearStreamDecryptor> audio_decryptor_;

  DISALLOW_COPY_AND_ASSIGN(FuchsiaDecryptor);
};

}  // namespace media

#endif  // MEDIA_FUCHSIA_CDM_FUCHSIA_DECRYPTOR_H_

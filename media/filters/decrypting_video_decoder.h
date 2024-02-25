// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_DECRYPTING_VIDEO_DECODER_H_
#define MEDIA_FILTERS_DECRYPTING_VIDEO_DECODER_H_

#include <stdint.h>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "media/base/callback_registry.h"
#include "media/base/cdm_context.h"
#include "media/base/decryptor.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"

namespace base {
class SequencedTaskRunner;
}

namespace media {

class DecoderBuffer;
class Decryptor;
class MediaLog;

// Decryptor-based VideoDecoder implementation that can decrypt and decode
// encrypted video buffers and return decrypted and decompressed video frames.
// All public APIs and callbacks are trampolined to the |task_runner_| so
// that no locks are required for thread safety.
class MEDIA_EXPORT DecryptingVideoDecoder : public VideoDecoder {
 public:
  DecryptingVideoDecoder(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      MediaLog* media_log);

  DecryptingVideoDecoder(const DecryptingVideoDecoder&) = delete;
  DecryptingVideoDecoder& operator=(const DecryptingVideoDecoder&) = delete;

  ~DecryptingVideoDecoder() override;

  bool SupportsDecryption() const override;

  // VideoDecoder implementation.
  VideoDecoderType GetDecoderType() const override;
  void Initialize(const VideoDecoderConfig& config,
                  bool low_delay,
                  CdmContext* cdm_context,
                  InitCB init_cb,
                  const OutputCB& output_cb,
                  const WaitingCB& waiting_cb) override;
  void Decode(scoped_refptr<DecoderBuffer> buffer, DecodeCB decode_cb) override;
  void Reset(base::OnceClosure closure) override;

  static const char kDecoderName[];

 private:
  // For a detailed state diagram please see this link: http://goo.gl/8jAok
  // TODO(xhwang): Add a ASCII state diagram in this file after this class
  // stabilizes.
  enum State {
    kUninitialized = 0,
    kPendingDecoderInit,
    kIdle,
    kPendingDecode,
    kWaitingForKey,
    kDecodeFinished,
    kError
  };

  // Callback for Decryptor::InitializeVideoDecoder() during initialization.
  void FinishInitialization(bool success);

  void DecodePendingBuffer();

  // Callback for Decryptor::DecryptAndDecodeVideo().
  void DeliverFrame(Decryptor::Status status, scoped_refptr<VideoFrame> frame);

  // Callback for the CDM to notify |this|.
  void OnCdmContextEvent(CdmContext::Event event);

  // Reset decoder and call |reset_cb_|.
  void DoReset();

  // Completes traces for various pending states.
  void CompletePendingDecode(Decryptor::Status status);
  void CompleteWaitingForDecryptionKey();

  bool HasClearLead() const { return has_clear_lead_.value_or(false); }

  // Set in constructor.
  scoped_refptr<base::SequencedTaskRunner> const task_runner_;
  const raw_ptr<MediaLog> media_log_;

  SEQUENCE_CHECKER(sequence_checker_);

  State state_ = kUninitialized;

  InitCB init_cb_;
  OutputCB output_cb_;
  DecodeCB decode_cb_;
  base::OnceClosure reset_cb_;
  WaitingCB waiting_cb_;

  VideoDecoderConfig config_;

  raw_ptr<Decryptor> decryptor_ = nullptr;

  // The buffer that needs decrypting/decoding.
  scoped_refptr<media::DecoderBuffer> pending_buffer_to_decode_;

  // Indicates the situation where new key is added during pending decode
  // (in other words, this variable can only be set in state kPendingDecode).
  // If this variable is true and kNoKey is returned then we need to try
  // decrypting/decoding again in case the newly added key is the correct
  // decryption key.
  bool key_added_while_decode_pending_ = false;

  // Once Initialized() with encrypted content support, if the stream changes to
  // clear content, we want to ensure this decoder remains used.
  bool support_clear_content_ = false;

  std::optional<bool> has_clear_lead_;

  bool switched_clear_to_encrypted_ = false;

  // To keep the CdmContext event callback registered.
  std::unique_ptr<CallbackRegistration> event_cb_registration_;

  base::WeakPtrFactory<DecryptingVideoDecoder> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_FILTERS_DECRYPTING_VIDEO_DECODER_H_

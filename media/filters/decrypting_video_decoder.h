// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_DECRYPTING_VIDEO_DECODER_H_
#define MEDIA_FILTERS_DECRYPTING_VIDEO_DECODER_H_

#include <stdint.h>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "media/base/cdm_context.h"
#include "media/base/decryptor.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"

namespace base {
class SingleThreadTaskRunner;
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
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      MediaLog* media_log);
  ~DecryptingVideoDecoder() override;

  // VideoDecoder implementation.
  std::string GetDisplayName() const override;
  void Initialize(
      const VideoDecoderConfig& config,
      bool low_delay,
      CdmContext* cdm_context,
      const InitCB& init_cb,
      const OutputCB& output_cb,
      const WaitingForDecryptionKeyCB& waiting_for_decryption_key_cb) override;
  void Decode(scoped_refptr<DecoderBuffer> buffer,
              const DecodeCB& decode_cb) override;
  void Reset(const base::Closure& closure) override;

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
  void DeliverFrame(Decryptor::Status status,
                    const scoped_refptr<VideoFrame>& frame);

  // Callback for the |decryptor_| to notify this object that a new key has been
  // added.
  void OnKeyAdded();

  // Reset decoder and call |reset_cb_|.
  void DoReset();

  // Completes traces for various pending states.
  void CompletePendingDecode(Decryptor::Status status);
  void CompleteWaitingForDecryptionKey();

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  MediaLog* media_log_;

  State state_;

  InitCB init_cb_;
  OutputCB output_cb_;
  DecodeCB decode_cb_;
  base::Closure reset_cb_;
  base::Closure waiting_for_decryption_key_cb_;

  VideoDecoderConfig config_;

  Decryptor* decryptor_;

  // The buffer that needs decrypting/decoding.
  scoped_refptr<media::DecoderBuffer> pending_buffer_to_decode_;

  // Indicates the situation where new key is added during pending decode
  // (in other words, this variable can only be set in state kPendingDecode).
  // If this variable is true and kNoKey is returned then we need to try
  // decrypting/decoding again in case the newly added key is the correct
  // decryption key.
  bool key_added_while_decode_pending_;

  // Once Initialized() with encrypted content support, if the stream changes to
  // clear content, we want to ensure this decoder remains used.
  bool support_clear_content_;

  base::WeakPtr<DecryptingVideoDecoder> weak_this_;
  base::WeakPtrFactory<DecryptingVideoDecoder> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DecryptingVideoDecoder);
};

}  // namespace media

#endif  // MEDIA_FILTERS_DECRYPTING_VIDEO_DECODER_H_

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_CHROMEOS_DECODER_BUFFER_TRANSCRYPTOR_H_
#define MEDIA_GPU_CHROMEOS_DECODER_BUFFER_TRANSCRYPTOR_H_

#include <memory>
#include <optional>

#include "base/containers/circular_deque.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "media/base/callback_registry.h"
#include "media/base/cdm_context.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decoder_status.h"
#include "media/base/decryptor.h"
#include "media/base/video_decoder.h"

namespace media {

class VideoDecoderMixin;

// This is used to send buffers to the CdmContext's Decryptor prior to sending
// them into the decoder for VideoDecoderPipeline. This is used in AMD
// implementations for protected content where the Decryptor normalizes the
// decryption before being passed into the HW decoders.
class DecoderBufferTranscryptor {
 public:
  using OnBufferTranscryptedCB =
      base::RepeatingCallback<void(scoped_refptr<DecoderBuffer>,
                                   VideoDecoder::DecodeCB)>;

  // The |transcrypt_callback| is invoked upon transcryption of a buffer. It
  // will be called with a nullptr in the event of failure.
  DecoderBufferTranscryptor(CdmContext* cdm_context,
                            VideoDecoderMixin& decoder,
                            bool needs_vp9_superframe_splitting,
                            OnBufferTranscryptedCB transcrypt_callback,
                            WaitingCB waiting_callback);
  DecoderBufferTranscryptor(const DecoderBufferTranscryptor&) = delete;
  DecoderBufferTranscryptor& operator=(const DecoderBufferTranscryptor&) =
      delete;
  ~DecoderBufferTranscryptor();

  // Enqueues a DecoderBuffer for transcryption. When complete, the callback
  // passed into the constructor will be invoked.
  void EnqueueBuffer(scoped_refptr<DecoderBuffer> buffer,
                     VideoDecoder::DecodeCB decode_cb);

  // Removes all pending tasks and invokes all pending VideoDecoder::DecodeCB
  // callbacks with the passed in |status|.
  void Reset(DecoderStatus status);

  // Invoked when more secure buffers might be available. When we have pending
  // tasks that are stalled due to secure buffer exhaustion, this is the
  // mechanism by which we will retry them.
  void SecureBuffersMayBeAvailable();

 private:
  // Transcrypt task holding single transcrypt request.
  struct TranscryptTask {
    TranscryptTask(scoped_refptr<DecoderBuffer> buffer,
                   VideoDecoder::DecodeCB decode_done_cb);
    TranscryptTask(const TranscryptTask&) = delete;
    TranscryptTask& operator=(const TranscryptTask&) = delete;
    ~TranscryptTask();
    TranscryptTask(TranscryptTask&&);
    TranscryptTask& operator=(TranscryptTask&&) = default;
    scoped_refptr<DecoderBuffer> buffer;
    VideoDecoder::DecodeCB decode_done_cb;
  };

  // Callback for the CDM to notify |this|.
  void OnCdmContextEvent(CdmContext::Event event);

  // Called to decrypt (i.e. transcrypt in our case) any pending buffers
  // available in the queue.
  void DecryptPendingBuffer();

  // Callback for the Decrypt call on transcryption.
  void OnBufferTranscrypted(Decryptor::Status status,
                            scoped_refptr<DecoderBuffer> transcrypted_buffer);

  void OnSecureBufferRelease(uint64_t secure_handle,
                             VideoDecoder::DecodeCB decode_cb,
                             DecoderStatus status);

  const raw_ref<VideoDecoderMixin> decoder_;  // Not owned.
  OnBufferTranscryptedCB transcrypt_callback_;
  WaitingCB waiting_callback_;

  // Indicates if a new usable key has become available while waiting for a
  // transcryption to complete. This allows us to detect if we need to retry
  // the transcryption if it fails due to the absence of a usable key.
  bool key_added_while_decrypting_ = false;

  // Queue containing all requested transcrypt tasks.
  base::circular_deque<TranscryptTask> transcrypt_task_queue_;
  // The transcrypt task we're currently trying to execute.
  std::optional<TranscryptTask> current_transcrypt_task_;

  // If true, then a request to the decryptor is in progress which means we
  // should not make another transcryption request until the pending one
  // completes (through a call to OnBufferTranscrypted()).
  // NOTE: The Decryptor implementation in use does support multiple
  // simultaneous calls to Decrypt, however we still throttle ourselves so we
  // don't end up with a backlog of Decrypt requests that need to be processed
  // before moving on after a Reset.
  bool transcrypt_pending_ = false;

  // If true, then we should split VP9 superframes up into individual frames
  // before decryption/decode.
  const bool needs_vp9_superframe_splitting_;

  // We need to use a CdmContextRef so that we destruct
  // |cdm_event_cb_registration_| before the CDM is destructed. The CDM has
  // mechanisms to ensure destruction on the proper thread.
  std::unique_ptr<CdmContextRef> cdm_context_ref_;

  // To keep the CdmContext event callback registered.
  std::unique_ptr<CallbackRegistration> cdm_event_cb_registration_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtr<DecoderBufferTranscryptor> weak_this_;
  base::WeakPtrFactory<DecoderBufferTranscryptor> weak_this_factory_{this};
};

}  // namespace media
#endif  // MEDIA_GPU_CHROMEOS_DECODER_BUFFER_TRANSCRYPTOR_H_

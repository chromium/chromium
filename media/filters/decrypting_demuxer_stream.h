// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_DECRYPTING_DEMUXER_STREAM_H_
#define MEDIA_FILTERS_DECRYPTING_DEMUXER_STREAM_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/cdm_context.h"
#include "media/base/decryptor.h"
#include "media/base/demuxer_stream.h"
#include "media/base/pipeline_status.h"
#include "media/base/video_decoder_config.h"
#include "media/base/waiting.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace media {

class DecoderBuffer;
class MediaLog;

// Decryptor-based DemuxerStream implementation that converts a potentially
// encrypted demuxer stream to a clear demuxer stream.
// All public APIs and callbacks are trampolined to the |task_runner_| so
// that no locks are required for thread safety.
class MEDIA_EXPORT DecryptingDemuxerStream : public DemuxerStream {
 public:
  DecryptingDemuxerStream(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      MediaLog* media_log,
      const WaitingCB& waiting_cb);

  // Cancels all pending operations immediately and fires all pending callbacks.
  ~DecryptingDemuxerStream() override;

  // |stream| must be encrypted and |cdm_context| must be non-null.
  void Initialize(DemuxerStream* stream,
                  CdmContext* cdm_context,
                  const PipelineStatusCB& status_cb);

  // Cancels all pending operations and fires all pending callbacks. If in
  // kPendingDemuxerRead or kPendingDecrypt state, waits for the pending
  // operation to finish before satisfying |closure|. Sets the state to
  // kUninitialized if |this| hasn't been initialized, or to kIdle otherwise.
  void Reset(const base::Closure& closure);

  // Returns the name of this class for logging purpose.
  std::string GetDisplayName() const;

  // DemuxerStream implementation.
  void Read(ReadCB read_cb) override;
  bool IsReadPending() const override;
  AudioDecoderConfig audio_decoder_config() override;
  VideoDecoderConfig video_decoder_config() override;
  Type type() const override;
  Liveness liveness() const override;
  void EnableBitstreamConverter() override;
  bool SupportsConfigChanges() override;

 private:
  // See this link for a detailed state diagram: http://shortn/_1nXgoVIrps
  // Each line has a number that corresponds to an action, status or function
  // that results in a state change. These actions, etc are all listed below.
  // NOTE: invoking Reset() will cause a transition from any state except
  //       kUninitialized to the kIdle state.
  //
  //    +----------------+         +---------------------------------+
  //    | kUninitialized |         | Any State Except kUninitialized |
  //    +----------------+         +---------------------------------+
  //             |                                  |
  //             0                                  7
  //             v                                  v
  //         +-------+                          +-------+
  //         | kIdle |<-------+-+               | kIdle |
  //         +-------+        | |               +-------+
  //             |            | |
  //             1            4 5
  //             v            | |
  //  +---------------------+ | |
  //  | kPendingDemuxerRead |-+ |
  //  +---------------------+   |
  //             |              |
  //             2              |
  //             v              |
  //    +-----------------+     |
  // +->| kPendingDecrypt |-----+
  // |  +-----------------+
  // |           |
  // 6           3
  // |           v
  // |   +----------------+
  // +---| kWaitingForKey |
  //     +----------------+
  //
  // 1) Read()
  // 2) Has encrypted buffer
  // 3) kNoKey
  // 4) kConfigChanged, kAborted, has clear buffer or end of stream
  // 5) kSuccess or kAborted
  // 6) OnKeyAdded()
  // 7) Reset()

  enum State {
    kUninitialized = 0,
    kIdle,
    kPendingDemuxerRead,
    kPendingDecrypt,
    kWaitingForKey
  };

  // Callback for DemuxerStream::Read().
  void OnBufferReadFromDemuxerStream(DemuxerStream::Status status,
                                     scoped_refptr<DecoderBuffer> buffer);

  void DecryptPendingBuffer();

  // Callback for Decryptor::Decrypt().
  void OnBufferDecrypted(Decryptor::Status status,
                         scoped_refptr<DecoderBuffer> decrypted_buffer);

  // Callback for the |decryptor_| to notify this object that a new key has been
  // added.
  void OnKeyAdded();

  // Resets decoder and calls |reset_cb_|.
  void DoReset();

  // Returns Decryptor::StreamType converted from |stream_type_|.
  Decryptor::StreamType GetDecryptorStreamType() const;

  // Creates and initializes either |audio_config_| or |video_config_| based on
  // |demuxer_stream_|.
  void InitializeDecoderConfig();

  // Completes traces for various pending states.
  void CompletePendingDecrypt(Decryptor::Status status);
  void CompleteWaitingForDecryptionKey();

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  MediaLog* const media_log_;
  WaitingCB waiting_cb_;

  State state_ = kUninitialized;

  PipelineStatusCB init_cb_;
  ReadCB read_cb_;
  base::Closure reset_cb_;

  // Pointer to the input demuxer stream that will feed us encrypted buffers.
  DemuxerStream* demuxer_stream_ = nullptr;

  AudioDecoderConfig audio_config_;
  VideoDecoderConfig video_config_;

  Decryptor* decryptor_ = nullptr;

  // The buffer returned by the demuxer that needs to be decrypted.
  scoped_refptr<media::DecoderBuffer> pending_buffer_to_decrypt_;

  // Indicates the situation where new key is added during pending decryption
  // (in other words, this variable can only be set in state kPendingDecrypt).
  // If this variable is true and kNoKey is returned then we need to try
  // decrypting again in case the newly added key is the correct decryption key.
  bool key_added_while_decrypt_pending_ = false;

  base::WeakPtr<DecryptingDemuxerStream> weak_this_;
  base::WeakPtrFactory<DecryptingDemuxerStream> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DecryptingDemuxerStream);
};

}  // namespace media

#endif  // MEDIA_FILTERS_DECRYPTING_DEMUXER_STREAM_H_

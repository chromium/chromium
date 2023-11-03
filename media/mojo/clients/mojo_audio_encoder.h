// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_CLIENTS_MOJO_AUDIO_ENCODER_H_
#define MEDIA_MOJO_CLIENTS_MOJO_AUDIO_ENCODER_H_

#include <list>
#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/audio_encoder.h"
#include "media/mojo/mojom/audio_encoder.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace media {

// An AudioEncoder that proxies to a mojom::AudioEncoder.
class MojoAudioEncoder final : public AudioEncoder,
                               public mojom::AudioEncoderClient {
 public:
  explicit MojoAudioEncoder(
      mojo::PendingRemote<mojom::AudioEncoder> remote_encoder);

  static bool IsSupported(AudioCodec codec);

  MojoAudioEncoder(const MojoAudioEncoder&) = delete;
  MojoAudioEncoder& operator=(const MojoAudioEncoder&) = delete;

  ~MojoAudioEncoder() final;

  // media::AudioEncoder implementation.
  void Initialize(const Options& options,
                  OutputCB output_cb,
                  EncoderStatusCB done_cb) final;

  void Encode(std::unique_ptr<AudioBus> audio_bus,
              base::TimeTicks capture_time,
              EncoderStatusCB done_cb) final;

  void Flush(EncoderStatusCB done_cb) final;

  // AudioEncoderClient implementation.
  void OnEncodedBufferReady(media::EncodedAudioBuffer buffer,
                            const CodecDescription& desc) final;

 private:
  // Using std::list here for stable iterators, so we can add and remove
  // pending callbacks without worry and nuke them all at once if need be
  // if Mojo connection error occurs.
  using PendingCallbacksList = std::list<EncoderStatusCB>;
  using PendingCallbackHandle = PendingCallbacksList::iterator;

  // It is different from regular EncoderStatusCB because mojo only gives us
  // `const EncoderStatus&` instead of `EncoderStatus`.
  using WrappedEncoderStatusCB =
      base::OnceCallback<void(const EncoderStatus& error)>;

  void CallAndReleaseCallback(PendingCallbackHandle handle,
                              const EncoderStatus& status);
  void CallAndReleaseAllPendingCallbacks(EncoderStatus status)
      VALID_CONTEXT_REQUIRED(sequence_checker_);
  WrappedEncoderStatusCB WrapCallbackAsPending(EncoderStatusCB callback)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  void BindRemote();
  void OnConnectionError();
  void PostStatusCallback(EncoderStatusCB callback, EncoderStatus status);

  SEQUENCE_CHECKER(sequence_checker_);
  mojo::PendingRemote<mojom::AudioEncoder> pending_remote_encoder_
      GUARDED_BY_CONTEXT(sequence_checker_);
  mojo::Remote<mojom::AudioEncoder> remote_encoder_
      GUARDED_BY_CONTEXT(sequence_checker_);
  mojo::AssociatedReceiver<mojom::AudioEncoderClient> client_receiver_{this};
  scoped_refptr<AudioBufferMemoryPool> buffer_pool_
      GUARDED_BY_CONTEXT(sequence_checker_);

  PendingCallbacksList pending_callbacks_ GUARDED_BY_CONTEXT(sequence_checker_);

  OutputCB output_cb_ GUARDED_BY_CONTEXT(sequence_checker_);
  Options options_ GUARDED_BY_CONTEXT(sequence_checker_);

  scoped_refptr<base::SequencedTaskRunner> runner_;
  base::WeakPtr<MojoAudioEncoder> weak_this_;
  base::WeakPtrFactory<MojoAudioEncoder> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_MOJO_CLIENTS_MOJO_AUDIO_ENCODER_H_

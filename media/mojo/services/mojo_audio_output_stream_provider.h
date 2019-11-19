// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_AUDIO_OUTPUT_STREAM_PROVIDER_H_
#define MEDIA_MOJO_SERVICES_MOJO_AUDIO_OUTPUT_STREAM_PROVIDER_H_

#include <memory>
#include <string>

#include "base/sequence_checker.h"
#include "media/audio/audio_output_delegate.h"
#include "media/mojo/mojom/audio_output_stream.mojom.h"
#include "media/mojo/services/media_mojo_export.h"
#include "media/mojo/services/mojo_audio_output_stream.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace media {

// Provides a single AudioOutput, given the audio parameters to use.
class MEDIA_MOJO_EXPORT MojoAudioOutputStreamProvider
    : public mojom::AudioOutputStreamProvider {
 public:
  using CreateDelegateCallback =
      base::OnceCallback<std::unique_ptr<AudioOutputDelegate>(
          const AudioParameters& params,
          mojo::PendingRemote<mojom::AudioOutputStreamObserver>,
          AudioOutputDelegate::EventHandler*)>;
  using DeleterCallback = base::OnceCallback<void(AudioOutputStreamProvider*)>;

  // |create_delegate_callback| is used to obtain an AudioOutputDelegate for the
  // AudioOutput when it's initialized and |deleter_callback| is called when
  // this class should be removed (stream ended/error). |deleter_callback| is
  // required to destroy |this| synchronously.
  MojoAudioOutputStreamProvider(
      mojo::PendingReceiver<mojom::AudioOutputStreamProvider> pending_receiver,
      CreateDelegateCallback create_delegate_callback,
      DeleterCallback deleter_callback,
      std::unique_ptr<mojom::AudioOutputStreamObserver> observer);

  ~MojoAudioOutputStreamProvider() override;

 private:
  // mojom::AudioOutputStreamProvider implementation.
  void Acquire(
      const AudioParameters& params,
      mojo::PendingRemote<mojom::AudioOutputStreamProviderClient>
          provider_client,
      const base::Optional<base::UnguessableToken>& processing_id) override;

  // Called when |audio_output_| had an error.
  void CleanUp(bool had_error);

  // Closes mojo connections, reports a bad message, and self-destructs.
  void BadMessage(const std::string& error);

  SEQUENCE_CHECKER(sequence_checker_);

  mojo::Receiver<AudioOutputStreamProvider> receiver_;
  CreateDelegateCallback create_delegate_callback_;
  DeleterCallback deleter_callback_;
  std::unique_ptr<mojom::AudioOutputStreamObserver> observer_;
  mojo::Receiver<mojom::AudioOutputStreamObserver> observer_receiver_;
  base::Optional<MojoAudioOutputStream> audio_output_;
  mojo::Remote<mojom::AudioOutputStreamProviderClient> provider_client_;

  DISALLOW_COPY_AND_ASSIGN(MojoAudioOutputStreamProvider);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_AUDIO_OUTPUT_STREAM_PROVIDER_H_

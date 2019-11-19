// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_AUDIO_OUTPUT_STREAM_BROKER_H_
#define CONTENT_BROWSER_MEDIA_AUDIO_OUTPUT_STREAM_BROKER_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/unguessable_token.h"
#include "content/browser/media/audio_stream_broker.h"
#include "content/browser/renderer_host/media/audio_output_stream_observer_impl.h"
#include "content/common/content_export.h"
#include "media/base/audio_parameters.h"
#include "media/mojo/mojom/audio_output_stream.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/audio/public/mojom/stream_factory.mojom.h"

namespace content {

// AudioOutputStreamBroker is used to broker a connection between a client
// (typically renderer) and the audio service. It also sets up all objects
// used for monitoring the stream.
class CONTENT_EXPORT AudioOutputStreamBroker final : public AudioStreamBroker {
 public:
  AudioOutputStreamBroker(
      int render_process_id,
      int render_frame_id,
      int stream_id,
      const std::string& output_device_id,
      const media::AudioParameters& params,
      const base::UnguessableToken& group_id,
      const base::Optional<base::UnguessableToken>& processing_id,
      DeleterCallback deleter,
      mojo::PendingRemote<media::mojom::AudioOutputStreamProviderClient>
          client);

  ~AudioOutputStreamBroker() final;

  // Creates the stream.
  void CreateStream(audio::mojom::StreamFactory* factory) final;

 private:
  using DisconnectReason =
      media::mojom::AudioOutputStreamObserver::DisconnectReason;

  void StreamCreated(
      mojo::PendingRemote<media::mojom::AudioOutputStream> stream,
      media::mojom::ReadWriteAudioDataPipePtr data_pipe);
  void ObserverBindingLost(uint32_t reason, const std::string& description);
  void Cleanup(DisconnectReason reason);
  bool AwaitingCreated() const;

  SEQUENCE_CHECKER(owning_sequence_);

  const std::string output_device_id_;
  const media::AudioParameters params_;
  const base::UnguessableToken group_id_;
  const base::Optional<base::UnguessableToken> processing_id_;

  // Set while CreateStream() has been called, but not StreamCreated().
  base::TimeTicks stream_creation_start_time_;

  DeleterCallback deleter_;

  mojo::Remote<media::mojom::AudioOutputStreamProviderClient> client_;

  AudioOutputStreamObserverImpl observer_;
  mojo::AssociatedReceiver<media::mojom::AudioOutputStreamObserver>
      observer_receiver_;

  DisconnectReason disconnect_reason_ = DisconnectReason::kDocumentDestroyed;

  base::WeakPtrFactory<AudioOutputStreamBroker> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AudioOutputStreamBroker);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_AUDIO_OUTPUT_STREAM_BROKER_H_

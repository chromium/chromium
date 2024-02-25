// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_AUDIO_INPUT_STREAM_BROKER_H_
#define CONTENT_BROWSER_MEDIA_AUDIO_INPUT_STREAM_BROKER_H_

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/audio_stream_broker.h"
#include "media/base/audio_parameters.h"
#include "media/mojo/mojom/audio_data_pipe.mojom.h"
#include "media/mojo/mojom/audio_input_stream.mojom.h"
#include "media/mojo/mojom/audio_stream_factory.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/media/renderer_audio_input_stream_factory.mojom.h"

namespace media {
class UserInputMonitorBase;
}

namespace content {

// AudioInputStreamBroker is used to broker a connection between a client
// (typically renderer) and the audio service. It is operated on the UI thread.
class CONTENT_EXPORT AudioInputStreamBroker final
    : public AudioStreamBroker,
      public media::mojom::AudioInputStreamObserver {
 public:
  AudioInputStreamBroker(
      int render_process_id,
      int render_frame_id,
      const std::string& device_id,
      const media::AudioParameters& params,
      uint32_t shared_memory_count,
      media::UserInputMonitorBase* user_input_monitor,
      bool enable_agc,
      media::mojom::AudioProcessingConfigPtr processing_config,
      AudioStreamBroker::DeleterCallback deleter,
      mojo::PendingRemote<blink::mojom::RendererAudioInputStreamFactoryClient>
          renderer_factory_client);

  AudioInputStreamBroker(const AudioInputStreamBroker&) = delete;
  AudioInputStreamBroker& operator=(const AudioInputStreamBroker&) = delete;

  ~AudioInputStreamBroker() final;

  // Creates the stream.
  void CreateStream(media::mojom::AudioStreamFactory* factory) final;

  // media::AudioInputStreamObserver implementation.
  void DidStartRecording() final;

 private:
  void StreamCreated(mojo::PendingRemote<media::mojom::AudioInputStream> stream,
                     media::mojom::ReadOnlyAudioDataPipePtr data_pipe,
                     bool initially_muted,
                     const std::optional<base::UnguessableToken>& stream_id);

  void ObserverBindingLost(uint32_t reason, const std::string& description);
  void ClientBindingLost();
  void Cleanup();

  const std::string device_id_;
  media::AudioParameters params_;
  const uint32_t shared_memory_count_;
  const raw_ptr<media::UserInputMonitorBase> user_input_monitor_;
  const bool enable_agc_;

  // Indicates that CreateStream has been called, but not StreamCreated.
  bool awaiting_created_ = false;

  DeleterCallback deleter_;

  media::mojom::AudioProcessingConfigPtr processing_config_;
  mojo::Remote<blink::mojom::RendererAudioInputStreamFactoryClient>
      renderer_factory_client_;
  mojo::Receiver<AudioInputStreamObserver> observer_receiver_{this};
  mojo::PendingReceiver<media::mojom::AudioInputStreamClient>
      pending_client_receiver_;

  media::mojom::AudioInputStreamObserver::DisconnectReason disconnect_reason_ =
      media::mojom::AudioInputStreamObserver::DisconnectReason::
          kDocumentDestroyed;

  base::WeakPtrFactory<AudioInputStreamBroker> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_AUDIO_INPUT_STREAM_BROKER_H_

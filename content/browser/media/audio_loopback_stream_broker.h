// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_AUDIO_LOOPBACK_STREAM_BROKER_H_
#define CONTENT_BROWSER_MEDIA_AUDIO_LOOPBACK_STREAM_BROKER_H_

#include <cstdint>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/media/audio_muting_session.h"
#include "content/common/content_export.h"
#include "content/public/browser/audio_stream_broker.h"
#include "media/base/audio_parameters.h"
#include "media/mojo/mojom/audio_data_pipe.mojom.h"
#include "media/mojo/mojom/audio_input_stream.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace media {
namespace mojom {
class AudioStreamFactory;
}
}  // namespace media

namespace content {

// AudioLoopbackStreamBroker is used to broker a connection between a client
// (typically renderer) and the audio service. It is operated on the UI thread.
class CONTENT_EXPORT AudioLoopbackStreamBroker final
    : public AudioStreamBroker,
      public media::mojom::AudioInputStreamObserver,
      public AudioStreamBroker::LoopbackSink {
 public:
  AudioLoopbackStreamBroker(
      int render_process_id,
      int render_frame_id,
      AudioStreamBroker::LoopbackSource* source,
      const media::AudioParameters& params,
      uint32_t shared_memory_count,
      bool mute_source,
      AudioStreamBroker::DeleterCallback deleter,
      mojo::PendingRemote<blink::mojom::RendererAudioInputStreamFactoryClient>
          renderer_factory_client);

  AudioLoopbackStreamBroker(const AudioLoopbackStreamBroker&) = delete;
  AudioLoopbackStreamBroker& operator=(const AudioLoopbackStreamBroker&) =
      delete;

  ~AudioLoopbackStreamBroker() final;

  // Creates the stream.
  void CreateStream(media::mojom::AudioStreamFactory* factory) final;

  // media::AudioInputStreamObserver implementation.
  void DidStartRecording() final;

  // AudioStreamBroker::LoopbackSink
  void OnSourceGone() final;

 private:
  void StreamCreated(mojo::PendingRemote<media::mojom::AudioInputStream> stream,
                     media::mojom::ReadOnlyAudioDataPipePtr data_pipe);
  void Cleanup();

  // Owner of the output streams to be looped back.
  raw_ptr<AudioStreamBroker::LoopbackSource> source_;

  const media::AudioParameters params_;
  const uint32_t shared_memory_count_;

  DeleterCallback deleter_;

  // Constructed only if the loopback source playback should be muted while the
  // loopback stream is running.
  std::optional<AudioMutingSession> muter_;

  mojo::Remote<blink::mojom::RendererAudioInputStreamFactoryClient>
      renderer_factory_client_;
  mojo::Receiver<AudioInputStreamObserver> observer_receiver_{this};
  mojo::PendingReceiver<media::mojom::AudioInputStreamClient> client_receiver_;

  base::WeakPtrFactory<AudioLoopbackStreamBroker> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_AUDIO_LOOPBACK_STREAM_BROKER_H_

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_AUDIO_LOOPBACK_STREAM_BROKER_H_
#define CONTENT_BROWSER_MEDIA_AUDIO_LOOPBACK_STREAM_BROKER_H_

#include <cstdint>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "content/browser/media/audio_muting_session.h"
#include "content/browser/media/audio_stream_broker.h"
#include "content/common/content_export.h"
#include "media/base/audio_parameters.h"
#include "media/mojo/mojom/audio_data_pipe.mojom.h"
#include "media/mojo/mojom/audio_input_stream.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace audio {
namespace mojom {
class StreamFactory;
}
}  // namespace audio

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
      mojo::PendingRemote<mojom::RendererAudioInputStreamFactoryClient>
          renderer_factory_client);

  ~AudioLoopbackStreamBroker() final;

  // Creates the stream.
  void CreateStream(audio::mojom::StreamFactory* factory) final;

  // media::AudioInputStreamObserver implementation.
  void DidStartRecording() final;

  // AudioStreamBroker::LoopbackSink
  void OnSourceGone() final;

 private:
  void StreamCreated(mojo::PendingRemote<media::mojom::AudioInputStream> stream,
                     media::mojom::ReadOnlyAudioDataPipePtr data_pipe);
  void Cleanup();

  // Owner of the output streams to be looped back.
  AudioStreamBroker::LoopbackSource* source_;

  const media::AudioParameters params_;
  const uint32_t shared_memory_count_;

  DeleterCallback deleter_;

  // Constructed only if the loopback source playback should be muted while the
  // loopback stream is running.
  base::Optional<AudioMutingSession> muter_;

  mojo::Remote<mojom::RendererAudioInputStreamFactoryClient>
      renderer_factory_client_;
  mojo::Receiver<AudioInputStreamObserver> observer_receiver_{this};
  mojo::PendingReceiver<media::mojom::AudioInputStreamClient> client_receiver_;

  base::WeakPtrFactory<AudioLoopbackStreamBroker> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AudioLoopbackStreamBroker);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_AUDIO_LOOPBACK_STREAM_BROKER_H_

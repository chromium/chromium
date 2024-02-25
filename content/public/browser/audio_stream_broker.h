// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_AUDIO_STREAM_BROKER_H_
#define CONTENT_PUBLIC_BROWSER_AUDIO_STREAM_BROKER_H_

#include <cstdint>
#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "content/common/content_export.h"
#include "media/mojo/mojom/audio_output_stream.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/media/renderer_audio_input_stream_factory.mojom.h"

namespace base {
class UnguessableToken;
}

namespace media {
class AudioParameters;
class UserInputMonitorBase;
namespace mojom {
class AudioStreamFactory;
}
}  // namespace media

namespace content {

// An AudioStreamBroker is used to broker a connection between a client
// (typically renderer) and the audio service. It also sets up all objects
// used for monitoring the stream. All AudioStreamBrokers are used on the IO
// thread.
class CONTENT_EXPORT AudioStreamBroker {
 public:
  class CONTENT_EXPORT LoopbackSink {
   public:
    LoopbackSink();

    LoopbackSink(const LoopbackSink&) = delete;
    LoopbackSink& operator=(const LoopbackSink&) = delete;

    virtual ~LoopbackSink();
    virtual void OnSourceGone() = 0;
  };

  class CONTENT_EXPORT LoopbackSource {
   public:
    LoopbackSource();

    LoopbackSource(const LoopbackSource&) = delete;
    LoopbackSource& operator=(const LoopbackSource&) = delete;

    virtual ~LoopbackSource();
    virtual void AddLoopbackSink(LoopbackSink* sink) = 0;
    virtual void RemoveLoopbackSink(LoopbackSink* sink) = 0;
    virtual const base::UnguessableToken& GetGroupID() = 0;
  };

  using DeleterCallback = base::OnceCallback<void(AudioStreamBroker*)>;

  AudioStreamBroker(int render_process_id, int render_frame_id);

  AudioStreamBroker(const AudioStreamBroker&) = delete;
  AudioStreamBroker& operator=(const AudioStreamBroker&) = delete;

  virtual ~AudioStreamBroker();

  virtual void CreateStream(media::mojom::AudioStreamFactory* factory) = 0;

  int render_process_id() const { return render_process_id_; }
  int render_frame_id() const { return render_frame_id_; }

 protected:
  const int render_process_id_;
  const int render_frame_id_;
};

// Used for dependency injection into ForwardingAudioStreamFactory. Used on the
// IO thread.
class CONTENT_EXPORT AudioStreamBrokerFactory {
 public:
  static std::unique_ptr<AudioStreamBrokerFactory> CreateImpl();

  AudioStreamBrokerFactory();

  AudioStreamBrokerFactory(const AudioStreamBrokerFactory&) = delete;
  AudioStreamBrokerFactory& operator=(const AudioStreamBrokerFactory&) = delete;

  virtual ~AudioStreamBrokerFactory();

  virtual std::unique_ptr<AudioStreamBroker> CreateAudioInputStreamBroker(
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
          renderer_factory_client) = 0;

  virtual std::unique_ptr<AudioStreamBroker> CreateAudioLoopbackStreamBroker(
      int render_process_id,
      int render_frame_id,
      AudioStreamBroker::LoopbackSource* source,
      const media::AudioParameters& params,
      uint32_t shared_memory_count,
      bool mute_source,
      AudioStreamBroker::DeleterCallback deleter,
      mojo::PendingRemote<blink::mojom::RendererAudioInputStreamFactoryClient>
          renderer_factory_client) = 0;

  virtual std::unique_ptr<AudioStreamBroker> CreateAudioOutputStreamBroker(
      int render_process_id,
      int render_frame_id,
      int stream_id,
      const std::string& output_device_id,
      const media::AudioParameters& params,
      const base::UnguessableToken& group_id,
      AudioStreamBroker::DeleterCallback deleter,
      mojo::PendingRemote<media::mojom::AudioOutputStreamProviderClient>
          client) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_AUDIO_STREAM_BROKER_H_

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_AUDIO_STREAM_BROKER_IMPL_H_
#define CONTENT_BROWSER_MEDIA_AUDIO_STREAM_BROKER_IMPL_H_

#include "content/public/browser/audio_stream_broker.h"

namespace content {

class AudioStreamBrokerFactoryImpl final : public AudioStreamBrokerFactory {
 public:
  AudioStreamBrokerFactoryImpl();
  ~AudioStreamBrokerFactoryImpl() final;

  std::unique_ptr<AudioStreamBroker> CreateAudioInputStreamBroker(
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
          renderer_factory_client) final;

  std::unique_ptr<AudioStreamBroker> CreateAudioLoopbackStreamBroker(
      int render_process_id,
      int render_frame_id,
      AudioStreamBroker::LoopbackSource* source,
      const media::AudioParameters& params,
      uint32_t shared_memory_count,
      bool mute_source,
      AudioStreamBroker::DeleterCallback deleter,
      mojo::PendingRemote<blink::mojom::RendererAudioInputStreamFactoryClient>
          renderer_factory_client) final;

  std::unique_ptr<AudioStreamBroker> CreateAudioOutputStreamBroker(
      int render_process_id,
      int render_frame_id,
      int stream_id,
      const std::string& output_device_id,
      const media::AudioParameters& params,
      const base::UnguessableToken& group_id,
      AudioStreamBroker::DeleterCallback deleter,
      mojo::PendingRemote<media::mojom::AudioOutputStreamProviderClient> client)
      final;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_AUDIO_STREAM_BROKER_IMPL_H_

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/audio_stream_broker.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/task/post_task.h"
#include "content/browser/media/audio_input_stream_broker.h"
#include "content/browser/media/audio_loopback_stream_broker.h"
#include "content/browser/media/audio_output_stream_broker.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"

namespace content {

namespace {

class AudioStreamBrokerFactoryImpl final : public AudioStreamBrokerFactory {
 public:
  AudioStreamBrokerFactoryImpl() = default;
  ~AudioStreamBrokerFactoryImpl() final = default;

  std::unique_ptr<AudioStreamBroker> CreateAudioInputStreamBroker(
      int render_process_id,
      int render_frame_id,
      const std::string& device_id,
      const media::AudioParameters& params,
      uint32_t shared_memory_count,
      media::UserInputMonitorBase* user_input_monitor,
      bool enable_agc,
      audio::mojom::AudioProcessingConfigPtr processing_config,
      AudioStreamBroker::DeleterCallback deleter,
      mojo::PendingRemote<mojom::RendererAudioInputStreamFactoryClient>
          renderer_factory_client) final {
    return std::make_unique<AudioInputStreamBroker>(
        render_process_id, render_frame_id, device_id, params,
        shared_memory_count, user_input_monitor, enable_agc,
        std::move(processing_config), std::move(deleter),
        std::move(renderer_factory_client));
  }

  std::unique_ptr<AudioStreamBroker> CreateAudioLoopbackStreamBroker(
      int render_process_id,
      int render_frame_id,
      AudioStreamBroker::LoopbackSource* source,
      const media::AudioParameters& params,
      uint32_t shared_memory_count,
      bool mute_source,
      AudioStreamBroker::DeleterCallback deleter,
      mojo::PendingRemote<mojom::RendererAudioInputStreamFactoryClient>
          renderer_factory_client) final {
    return std::make_unique<AudioLoopbackStreamBroker>(
        render_process_id, render_frame_id, source, params, shared_memory_count,
        mute_source, std::move(deleter), std::move(renderer_factory_client));
  }

  std::unique_ptr<AudioStreamBroker> CreateAudioOutputStreamBroker(
      int render_process_id,
      int render_frame_id,
      int stream_id,
      const std::string& output_device_id,
      const media::AudioParameters& params,
      const base::UnguessableToken& group_id,
      const base::Optional<base::UnguessableToken>& processing_id,
      AudioStreamBroker::DeleterCallback deleter,
      mojo::PendingRemote<media::mojom::AudioOutputStreamProviderClient> client)
      final {
    return std::make_unique<AudioOutputStreamBroker>(
        render_process_id, render_frame_id, stream_id, output_device_id, params,
        group_id, processing_id, std::move(deleter), std::move(client));
  }
};

}  // namespace

AudioStreamBroker::LoopbackSink::LoopbackSink() = default;
AudioStreamBroker::LoopbackSink::~LoopbackSink() = default;

AudioStreamBroker::LoopbackSource::LoopbackSource() = default;
AudioStreamBroker::LoopbackSource::~LoopbackSource() = default;

AudioStreamBroker::AudioStreamBroker(int render_process_id, int render_frame_id)
    : render_process_id_(render_process_id),
      render_frame_id_(render_frame_id) {}
AudioStreamBroker::~AudioStreamBroker() {}

// static
void AudioStreamBroker::NotifyProcessHostOfStartedStream(
    int render_process_id) {
  auto impl = [](int id) {
    if (auto* process_host = RenderProcessHost::FromID(id))
      process_host->OnMediaStreamAdded();
  };
  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(impl, render_process_id));
}

// static
void AudioStreamBroker::NotifyProcessHostOfStoppedStream(
    int render_process_id) {
  auto impl = [](int id) {
    if (auto* process_host = RenderProcessHost::FromID(id))
      process_host->OnMediaStreamRemoved();
  };
  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(impl, render_process_id));
}

AudioStreamBrokerFactory::AudioStreamBrokerFactory() {}
AudioStreamBrokerFactory::~AudioStreamBrokerFactory() {}

// static
std::unique_ptr<AudioStreamBrokerFactory>
AudioStreamBrokerFactory::CreateImpl() {
  return std::make_unique<AudioStreamBrokerFactoryImpl>();
}

}  // namespace content

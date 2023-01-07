// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/audio_stream_broker_impl.h"

#include <utility>

#include "content/browser/media/audio_input_stream_broker.h"
#include "content/browser/media/audio_loopback_stream_broker.h"
#include "content/browser/media/audio_output_stream_broker.h"
#include "content/public/browser/audio_stream_broker.h"
#include "media/mojo/mojom/audio_processing.mojom.h"

namespace content {

AudioStreamBrokerFactoryImpl::AudioStreamBrokerFactoryImpl() = default;
AudioStreamBrokerFactoryImpl::~AudioStreamBrokerFactoryImpl() = default;

std::unique_ptr<AudioStreamBroker>
AudioStreamBrokerFactoryImpl::CreateAudioInputStreamBroker(
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
        renderer_factory_client) {
  return std::make_unique<AudioInputStreamBroker>(
      render_process_id, render_frame_id, device_id, params,
      shared_memory_count, user_input_monitor, enable_agc,
      std::move(processing_config), std::move(deleter),
      std::move(renderer_factory_client));
}

std::unique_ptr<AudioStreamBroker>
AudioStreamBrokerFactoryImpl::CreateAudioLoopbackStreamBroker(
    int render_process_id,
    int render_frame_id,
    AudioStreamBroker::LoopbackSource* source,
    const media::AudioParameters& params,
    uint32_t shared_memory_count,
    bool mute_source,
    AudioStreamBroker::DeleterCallback deleter,
    mojo::PendingRemote<blink::mojom::RendererAudioInputStreamFactoryClient>
        renderer_factory_client) {
  return std::make_unique<AudioLoopbackStreamBroker>(
      render_process_id, render_frame_id, source, params, shared_memory_count,
      mute_source, std::move(deleter), std::move(renderer_factory_client));
}

std::unique_ptr<AudioStreamBroker>
AudioStreamBrokerFactoryImpl::CreateAudioOutputStreamBroker(
    int render_process_id,
    int render_frame_id,
    int stream_id,
    const std::string& output_device_id,
    const media::AudioParameters& params,
    const base::UnguessableToken& group_id,
    AudioStreamBroker::DeleterCallback deleter,
    mojo::PendingRemote<media::mojom::AudioOutputStreamProviderClient> client) {
  return std::make_unique<AudioOutputStreamBroker>(
      render_process_id, render_frame_id, stream_id, output_device_id, params,
      group_id, std::move(deleter), std::move(client));
}

// static
std::unique_ptr<AudioStreamBrokerFactory>
AudioStreamBrokerFactory::CreateImpl() {
  return std::make_unique<AudioStreamBrokerFactoryImpl>();
}

}  // namespace content

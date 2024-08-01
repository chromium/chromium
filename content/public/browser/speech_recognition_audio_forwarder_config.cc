// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/speech_recognition_audio_forwarder_config.h"

#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {

SpeechRecognitionAudioForwarderConfig::SpeechRecognitionAudioForwarderConfig(
    mojo::PendingReceiver<media::mojom::SpeechRecognitionAudioForwarder>
        audio_forwarder,
    int channel_count,
    int sample_rate)
    : audio_forwarder(std::move(audio_forwarder)),
      channel_count(channel_count),
      sample_rate(sample_rate) {}

SpeechRecognitionAudioForwarderConfig::SpeechRecognitionAudioForwarderConfig(
    SpeechRecognitionAudioForwarderConfig& other)
    : audio_forwarder(other.audio_forwarder.PassPipe()),
      channel_count(other.channel_count),
      sample_rate(other.sample_rate) {}

SpeechRecognitionAudioForwarderConfig::
    ~SpeechRecognitionAudioForwarderConfig() = default;

}  // namespace content

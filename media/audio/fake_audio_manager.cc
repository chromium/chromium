// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/fake_audio_manager.h"

#include <algorithm>
#include <utility>
#include "media/base/audio_parameters.h"
#include "media/base/channel_layout.h"

namespace media {

namespace {

const int kDefaultInputBufferSize = 1024;
const int kDefaultSampleRate = 48000;

}  // namespace

FakeAudioManager::FakeAudioManager(std::unique_ptr<AudioThread> audio_thread,
                                   AudioLogFactory* audio_log_factory)
    : AudioManagerBase(std::move(audio_thread), audio_log_factory) {}

FakeAudioManager::~FakeAudioManager() = default;

// Implementation of AudioManager.
bool FakeAudioManager::HasAudioOutputDevices() { return false; }

bool FakeAudioManager::HasAudioInputDevices() { return false; }

const char* FakeAudioManager::GetName() {
  return "Fake";
}

// Implementation of AudioManagerBase.
AudioOutputStream* FakeAudioManager::MakeLinearOutputStream(
    const AudioParameters& params,
    const LogCallback& log_callback) {
  return FakeAudioOutputStream::MakeFakeStream(this, params);
}

AudioOutputStream* FakeAudioManager::MakeLowLatencyOutputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  return FakeAudioOutputStream::MakeFakeStream(this, params);
}

AudioInputStream* FakeAudioManager::MakeLinearInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  return FakeAudioInputStream::MakeFakeStream(this, params);
}

AudioInputStream* FakeAudioManager::MakeLowLatencyInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  return FakeAudioInputStream::MakeFakeStream(this, params);
}

AudioParameters FakeAudioManager::GetPreferredOutputStreamParameters(
    const std::string& output_device_id,
    const AudioParameters& input_params) {
  static const int kDefaultOutputBufferSize = 2048;
  ChannelLayoutConfig channel_layout_config = ChannelLayoutConfig::Stereo();
  int sample_rate = kDefaultSampleRate;
  int buffer_size = kDefaultOutputBufferSize;
  if (input_params.IsValid()) {
    sample_rate = input_params.sample_rate();
    channel_layout_config = input_params.channel_layout_config();
    buffer_size = std::min(input_params.frames_per_buffer(), buffer_size);
  }

  return AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                         channel_layout_config, sample_rate, buffer_size);
}

AudioParameters FakeAudioManager::GetInputStreamParameters(
    const std::string& device_id) {
  return AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                         ChannelLayoutConfig::Stereo(), kDefaultSampleRate,
                         kDefaultInputBufferSize);
}

}  // namespace media

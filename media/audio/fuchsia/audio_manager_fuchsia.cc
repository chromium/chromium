// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/fuchsia/audio_manager_fuchsia.h"

#include <memory>

#include "base/command_line.h"
#include "media/audio/fuchsia/audio_output_stream_fuchsia.h"
#include "media/base/media_switches.h"

namespace media {

AudioManagerFuchsia::AudioManagerFuchsia(
    std::unique_ptr<AudioThread> audio_thread,
    AudioLogFactory* audio_log_factory)
    : AudioManagerBase(std::move(audio_thread), audio_log_factory) {}

AudioManagerFuchsia::~AudioManagerFuchsia() = default;

bool AudioManagerFuchsia::HasAudioOutputDevices() {
  // TODO(crbug.com/852834): Fuchsia currently doesn't provide an API for device
  // enumeration. Update this method when that functionality is implemented.
  return true;
}

bool AudioManagerFuchsia::HasAudioInputDevices() {
  // TODO(crbug.com/852834): Fuchsia currently doesn't provide an API for device
  // enumeration. Update this method when that functionality is implemented.
  return true;
}

void AudioManagerFuchsia::GetAudioInputDeviceNames(
    AudioDeviceNames* device_names) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableAudioInput)) {
    return;
  }

  // TODO(crbug.com/852834): Fuchsia currently doesn't provide an API for device
  // enumeration. Update this method when that functionality is implemented.
  *device_names = {AudioDeviceName::CreateDefault()};
}

void AudioManagerFuchsia::GetAudioOutputDeviceNames(
    AudioDeviceNames* device_names) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableAudioOutput)) {
    return;
  }

  // TODO(crbug.com/852834): Fuchsia currently doesn't provide an API for device
  // enumeration. Update this method when that functionality is implemented.
  *device_names = {AudioDeviceName::CreateDefault()};
}

AudioParameters AudioManagerFuchsia::GetInputStreamParameters(
    const std::string& device_id) {
  // TODO(crbug.com/852834): Fuchsia currently doesn't provide an API to get
  // device configuration and supported effects. Update this method when that
  // functionality is implemented.
  //
  // Use 16kHz sample rate with 10ms buffer, which is consistent with
  // the default configuration used in the AudioCapturer implementation.
  // Assume that the system-provided AudioConsumer supports echo cancellation,
  // noise suppression and automatic gain control.
  AudioParameters params(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                         CHANNEL_LAYOUT_MONO, 16000, 160);
  params.set_effects(AudioParameters::ECHO_CANCELLER |
                     AudioParameters::NOISE_SUPPRESSION |
                     AudioParameters::AUTOMATIC_GAIN_CONTROL);

  return params;
}

AudioParameters AudioManagerFuchsia::GetPreferredOutputStreamParameters(
    const std::string& output_device_id,
    const AudioParameters& input_params) {
  // TODO(crbug.com/852834): Fuchsia currently doesn't provide an API to get
  // device configuration. Update this method when that functionality is
  // implemented.
  return AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                         CHANNEL_LAYOUT_STEREO, 48000, 480);
}

const char* AudioManagerFuchsia::GetName() {
  return "Fuchsia";
}

AudioOutputStream* AudioManagerFuchsia::MakeLinearOutputStream(
    const AudioParameters& params,
    const LogCallback& log_callback) {
  NOTREACHED();
  return nullptr;
}

AudioOutputStream* AudioManagerFuchsia::MakeLowLatencyOutputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());

  if (!device_id.empty() &&
      device_id != AudioDeviceDescription::kDefaultDeviceId) {
    return nullptr;
  }

  return new AudioOutputStreamFuchsia(this, params);
}

AudioInputStream* AudioManagerFuchsia::MakeLinearInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  NOTREACHED();
  return nullptr;
}

AudioInputStream* AudioManagerFuchsia::MakeLowLatencyInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  NOTREACHED();
  return nullptr;
}

std::unique_ptr<AudioManager> CreateAudioManager(
    std::unique_ptr<AudioThread> audio_thread,
    AudioLogFactory* audio_log_factory) {
  return std::make_unique<AudioManagerFuchsia>(std::move(audio_thread),
                                               audio_log_factory);
}

}  // namespace media

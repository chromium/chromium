// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/ios/audio_manager_ios.h"

#include <memory>

#include "media/audio/ios/audio_session_manager_ios.h"
#include "media/audio/mac/audio_auhal_mac.h"

namespace media {

// Default sample-rate and buffer size.
constexpr int kDefaultSampleRate = 44100;
constexpr int kDefaultInputBufferSize = 1024;

std::unique_ptr<media::AudioManager> CreateAudioManager(
    std::unique_ptr<AudioThread> audio_thread,
    AudioLogFactory* audio_log_factory) {
  return std::make_unique<AudioManagerIOS>(std::move(audio_thread),
                                           audio_log_factory);
}

AudioManagerIOS::AudioManagerIOS(std::unique_ptr<AudioThread> audio_thread,
                                 AudioLogFactory* audio_log_factory)
    : AudioManagerBase(std::move(audio_thread), audio_log_factory) {
  audio_session_manager_ = std::make_unique<AudioSessionManagerIOS>();
}

AudioManagerIOS::~AudioManagerIOS() = default;

bool AudioManagerIOS::HasAudioOutputDevices() {
  return audio_session_manager_->HasAudioHardware(/*is_input=*/false);
}

bool AudioManagerIOS::HasAudioInputDevices() {
  return audio_session_manager_->HasAudioHardware(/*is_input=*/true);
}

void AudioManagerIOS::GetAudioInputDeviceNames(AudioDeviceNames* device_names) {
  DCHECK(device_names->empty());
  audio_session_manager_->GetAudioDeviceInfo(true, device_names);
}

void AudioManagerIOS::GetAudioOutputDeviceNames(
    AudioDeviceNames* device_names) {
  DCHECK(device_names->empty());
  audio_session_manager_->GetAudioDeviceInfo(false, device_names);
}

AudioParameters AudioManagerIOS::GetInputStreamParameters(
    const std::string& device_id) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  return AudioParameters(AudioParameters::AUDIO_PCM_LINEAR,
                         ChannelLayoutConfig::Stereo(), kDefaultSampleRate,
                         kDefaultInputBufferSize);
}

std::string AudioManagerIOS::GetAssociatedOutputDeviceID(
    const std::string& input_device_unique_id) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  return std::string();
}

const char* AudioManagerIOS::GetName() {
  return "iOS";
}

AudioOutputStream* AudioManagerIOS::MakeLinearOutputStream(
    const AudioParameters& params,
    const LogCallback& log_callback) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  return MakeLowLatencyOutputStream(params, std::string(), log_callback);
}

AudioOutputStream* AudioManagerIOS::MakeLowLatencyOutputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());

  AUHALStream* stream =
      new AUHALStream(this, params, kAudioObjectUnknown, log_callback);
  output_streams_.push_back(stream);
  return stream;
}

AudioInputStream* AudioManagerIOS::MakeLinearInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LINEAR, params.format());
  NOTIMPLEMENTED();
  return nullptr;
}

AudioInputStream* AudioManagerIOS::MakeLowLatencyInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());
  NOTIMPLEMENTED();
  return nullptr;
}

void AudioManagerIOS::ReleaseOutputStreamUsingRealDevice(
    AudioOutputStream* stream,
    AudioDeviceID device_id) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  output_streams_.remove(static_cast<AUHALStream*>(stream));
  AudioManagerBase::ReleaseOutputStream(stream);
}

bool AudioManagerIOS::MaybeChangeBufferSize(AudioDeviceID device_id,
                                            AudioUnit audio_unit,
                                            AudioUnitElement element,
                                            size_t desired_buffer_size) {
  return true;
}

AudioParameters AudioManagerIOS::GetPreferredOutputStreamParameters(
    const std::string& output_device_id,
    const AudioParameters& input_params) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  return AudioParameters(AudioParameters::AUDIO_PCM_LINEAR,
                         ChannelLayoutConfig::Stereo(), kDefaultSampleRate,
                         kDefaultInputBufferSize);
}

}  // namespace media

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/ios/audio_manager_ios.h"

#include <memory>

#include "media/audio/ios/audio_session_manager_ios.h"
#include "media/audio/mac/audio_auhal_mac.h"
#include "media/audio/mac/audio_input_mac.h"

namespace media {

// Default buffer size.
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
    const std::string& input_device_id) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  int sample_rate = audio_session_manager_->HardwareSampleRate();
  return AudioParameters(AudioParameters::AUDIO_PCM_LINEAR,
                         ChannelLayoutConfig::Stereo(), sample_rate,
                         kDefaultInputBufferSize);
}

std::string AudioManagerIOS::GetAssociatedOutputDeviceID(
    const std::string& input_device_unique_id) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  return std::string();
}

const char* media::AudioManagerIOS::GetName() {
  return "iOS";
}

void AudioManagerIOS::ReleaseOutputStreamUsingRealDevice(
    AudioOutputStream* stream,
    AudioDeviceID device_id) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  output_streams_.remove(static_cast<AUHALStream*>(stream));
  AudioManagerBase::ReleaseOutputStream(stream);
}

void AudioManagerIOS::ReleaseInputStreamUsingRealDevice(
    AudioInputStream* stream) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  auto stream_it = base::ranges::find(basic_input_streams_, stream);
  if (stream_it != basic_input_streams_.end()) {
    basic_input_streams_.erase(stream_it);
  }

  AudioManagerBase::ReleaseInputStream(stream);
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
  AudioInputStream* stream = new PCMQueueInAudioInputStream(this, params);
  basic_input_streams_.push_back(stream);
  return stream;
}

AudioInputStream* AudioManagerIOS::MakeLowLatencyInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  // TODO: Replace PCMQueueInAudioInputStream with AUAudioInputStream to use
  // low-latency implementation.
  return MakeLinearInputStream(params, device_id, log_callback);
}

std::string AudioManagerIOS::GetDefaultInputDeviceID() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  return audio_session_manager_->GetDefaultInputDeviceID();
}

std::string AudioManagerIOS::GetDefaultOutputDeviceID() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  return audio_session_manager_->GetDefaultOutputDeviceID();
}

bool AudioManagerIOS::MaybeChangeBufferSize(AudioDeviceID device_id,
                                            AudioUnit audio_unit,
                                            AudioUnitElement element,
                                            size_t desired_buffer_size) {
  // TODO: Add IO buffer size  handling based on `desired_buffer_size`. Refer
  // comments at audio_manager_mac.h for more information.
  return true;
}

float AudioManagerIOS::GetInputGain() {
  return audio_session_manager_->GetInputGain();
}

bool AudioManagerIOS::SetInputGain(float volume) {
  return audio_session_manager_->SetInputGain(volume);
}

bool AudioManagerIOS::IsInputMuted() {
  return audio_session_manager_->IsInputMuted();
}

// private
AudioParameters AudioManagerIOS::GetPreferredOutputStreamParameters(
    const std::string& output_device_id,
    const AudioParameters& input_params) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  int sample_rate = audio_session_manager_->HardwareSampleRate();
  return AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                         ChannelLayoutConfig::Stereo(), sample_rate,
                         kDefaultInputBufferSize);
}

}  // namespace media

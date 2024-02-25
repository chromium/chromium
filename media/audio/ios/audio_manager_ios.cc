// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/ios/audio_manager_ios.h"

#include <memory>

#include "media/audio/apple/audio_input.h"
#include "media/audio/apple/audio_low_latency_input.h"
#include "media/audio/apple/audio_manager_apple.h"
#include "media/audio/ios/audio_session_manager_ios.h"

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
    : AudioManagerApple(std::move(audio_thread), audio_log_factory) {
  AudioSessionManagerIOS::GetInstance().SetActive(true);
}

AudioManagerIOS::~AudioManagerIOS() {
  AudioSessionManagerIOS::GetInstance().SetActive(false);
}

bool AudioManagerIOS::HasAudioOutputDevices() {
  return AudioSessionManagerIOS::GetInstance().HasAudioHardware(
      /*is_input=*/false);
}

bool AudioManagerIOS::HasAudioInputDevices() {
  return AudioSessionManagerIOS::GetInstance().HasAudioHardware(
      /*is_input=*/true);
}

void AudioManagerIOS::GetAudioInputDeviceNames(AudioDeviceNames* device_names) {
  DCHECK(device_names->empty());
  AudioSessionManagerIOS::GetInstance().GetAudioDeviceInfo(true, device_names);
}

void AudioManagerIOS::GetAudioOutputDeviceNames(
    AudioDeviceNames* device_names) {
  DCHECK(device_names->empty());
  AudioSessionManagerIOS::GetInstance().GetAudioDeviceInfo(false, device_names);
}

AudioParameters AudioManagerIOS::GetInputStreamParameters(
    const std::string& input_device_id) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  const int hardware_sample_rate =
      AudioSessionManagerIOS::GetInstance().HardwareSampleRate();

  // Try to use mono to save resources. Also avoids channel format conversion in
  // the I/O audio unit.
  ChannelLayoutConfig channel_layout_config = ChannelLayoutConfig::Mono();

  AudioParameters params = AudioParameters(
      AudioParameters::AUDIO_PCM_LOW_LATENCY, channel_layout_config,
      hardware_sample_rate, kDefaultInputBufferSize);
  return params;
}

std::string AudioManagerIOS::GetAssociatedOutputDeviceID(
    const std::string& input_device_unique_id) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  return std::string();
}

const char* media::AudioManagerIOS::GetName() {
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
  return stream;
}

AudioInputStream* AudioManagerIOS::MakeLinearInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  AudioInputStream* stream = new PCMQueueInAudioInputStream(this, params);
  return stream;
}

AudioInputStream* AudioManagerIOS::MakeLowLatencyInputStream(
    const AudioParameters& params,
    const std::string& device_id,
    const LogCallback& log_callback) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());

  VoiceProcessingMode voice_processing_mode = VoiceProcessingMode::kDisabled;

  auto* stream = new AUAudioInputStream(this, params, kAudioObjectUnknown,
                                        log_callback, voice_processing_mode);
  return stream;
}

std::string AudioManagerIOS::GetDefaultInputDeviceID() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  return AudioSessionManagerIOS::GetInstance().GetDefaultInputDeviceID();
}

std::string AudioManagerIOS::GetDefaultOutputDeviceID() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  return AudioSessionManagerIOS::GetInstance().GetDefaultOutputDeviceID();
}

bool AudioManagerIOS::MaybeChangeBufferSize(AudioDeviceID device_id,
                                            AudioUnit audio_unit,
                                            AudioUnitElement element,
                                            size_t desired_buffer_size) {
  // TODO: Add IO buffer size  handling based on `desired_buffer_size`. Refer
  // comments at audio_manager_mac.h for more information.
  return true;
}

bool AudioManagerIOS::DeviceSupportsAmbientNoiseReduction(
    AudioDeviceID device_id) {
  return false;
}

bool AudioManagerIOS::SuppressNoiseReduction(AudioDeviceID device_id) {
  return false;
}

void AudioManagerIOS::UnsuppressNoiseReduction(AudioDeviceID device_id) {
  NOTIMPLEMENTED();
}

double AudioManagerIOS::GetMaxInputVolume(AudioDeviceID device_id) {
  return 1.0;
}

bool AudioManagerIOS::ShouldDeferStreamStart() const {
  return false;
}

// static
double AudioManagerIOS::HardwareIOBufferDuration() {
  return AudioSessionManagerIOS::GetInstance().HardwareIOBufferDuration();
}

double AudioManagerIOS::HardwareLatency(bool is_input) {
  return AudioSessionManagerIOS::GetInstance().HardwareLatency(is_input);
}

long AudioManagerIOS::GetDeviceChannels(bool is_input) {
  return AudioSessionManagerIOS::GetInstance().GetDeviceChannels(is_input);
}

double AudioManagerIOS::GetInputVolume(AudioDeviceID device_id) {
  return static_cast<double>(
      AudioSessionManagerIOS::GetInstance().GetInputGain());
}

void AudioManagerIOS::SetInputVolume(AudioDeviceID device_id, double volume) {
  AudioSessionManagerIOS::GetInstance().SetInputGain(
      static_cast<float>(volume));
}

bool AudioManagerIOS::IsInputMuted(AudioDeviceID device_id) {
  return AudioSessionManagerIOS::GetInstance().IsInputMuted();
}

int AudioManagerIOS::HardwareSampleRateForDevice(AudioDeviceID device_id) {
  return static_cast<int>(
      AudioSessionManagerIOS::GetInstance().HardwareSampleRate());
}

bool AudioManagerIOS::IsInputGainSettable() {
  return AudioSessionManagerIOS::GetInstance().IsInputGainSettable();
}

OSStatus AudioManagerIOS::GetInputDeviceStreamFormat(
    AudioUnit audio_unit,
    AudioStreamBasicDescription* input_format) {
  // Configure audio stream format for 16-bit PCM
  const SampleFormat kSampleFormat = kSampleFormatS16;
  input_format->mSampleRate =
      AudioSessionManagerIOS::GetInstance().HardwareSampleRate();
  input_format->mFormatID = kAudioFormatLinearPCM;
  input_format->mFormatFlags =
      kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
  input_format->mChannelsPerFrame =
      AudioSessionManagerIOS::GetInstance().GetDeviceChannels(
          /*is_input=*/true);
  input_format->mBitsPerChannel = SampleFormatToBitsPerChannel(kSampleFormat);

  // Calculate other fields based on the above settings
  input_format->mBytesPerPacket = SampleFormatToBytesPerChannel(kSampleFormat) *
                                  input_format->mChannelsPerFrame;
  input_format->mBytesPerFrame = input_format->mBytesPerPacket;
  input_format->mFramesPerPacket = 1;  // uncompressed audio
  return noErr;
}

// protected
AudioParameters AudioManagerIOS::GetPreferredOutputStreamParameters(
    const std::string& output_device_id,
    const AudioParameters& input_params) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());
  const bool has_valid_input_params = input_params.IsValid();
  const int hardware_sample_rate =
      AudioSessionManagerIOS::GetInstance().HardwareSampleRate();

  const int hardware_channels =
      AudioSessionManagerIOS::GetInstance().GetDeviceChannels(
          /*is_input=*/false);

  // Use the input channel count and channel layout if possible.  Let OSX take
  // care of remapping the channels; this lets user specified channel layouts
  // work correctly.
  int output_channels = input_params.channels();
  ChannelLayout channel_layout = input_params.channel_layout();
  if (!has_valid_input_params || output_channels > hardware_channels) {
    output_channels = hardware_channels;
    channel_layout = GuessChannelLayout(output_channels);
    if (channel_layout == CHANNEL_LAYOUT_UNSUPPORTED) {
      channel_layout = CHANNEL_LAYOUT_DISCRETE;
    }
  }

  AudioParameters params(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                         {channel_layout, output_channels},
                         hardware_sample_rate, kDefaultInputBufferSize);
  return params;
}

}  // namespace media

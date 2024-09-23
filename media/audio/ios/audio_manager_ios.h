// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_IOS_AUDIO_MANAGER_IOS_H_
#define MEDIA_AUDIO_IOS_AUDIO_MANAGER_IOS_H_

#include <stddef.h>

#include <list>
#include <map>
#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "media/audio/apple/audio_auhal.h"
#include "media/audio/apple/audio_manager_apple.h"
#include "media/audio/audio_manager_base.h"
#include "media/audio/fake_audio_manager.h"

namespace media {

// iOS implementation of the AudioManager singleton. This class is internal
// to the audio output and only internal users can call methods not exposed by
// the AudioManager class.
// TODO(crbug.com/40255660): Fill this implementation out.
class MEDIA_EXPORT AudioManagerIOS : public AudioManagerApple {
 public:
  AudioManagerIOS(std::unique_ptr<AudioThread> audio_thread,
                  AudioLogFactory* audio_log_factory);

  AudioManagerIOS(const AudioManagerIOS&) = delete;
  AudioManagerIOS& operator=(const AudioManagerIOS&) = delete;

  ~AudioManagerIOS() override;

  // Implementation of AudioManager.
  bool HasAudioOutputDevices() override;
  bool HasAudioInputDevices() override;
  void GetAudioInputDeviceNames(AudioDeviceNames* device_names) override;
  void GetAudioOutputDeviceNames(AudioDeviceNames* device_names) override;
  AudioParameters GetInputStreamParameters(
      const std::string& input_device_id) override;
  std::string GetAssociatedOutputDeviceID(
      const std::string& input_device_id) override;
  const char* GetName() override;

  // Implementation of AudioManagerBase.
  AudioOutputStream* MakeLinearOutputStream(
      const AudioParameters& params,
      const LogCallback& log_callback) override;
  AudioOutputStream* MakeLowLatencyOutputStream(
      const AudioParameters& params,
      const std::string& device_id,
      const LogCallback& log_callback) override;
  AudioInputStream* MakeLinearInputStream(
      const AudioParameters& params,
      const std::string& device_id,
      const LogCallback& log_callback) override;
  AudioInputStream* MakeLowLatencyInputStream(
      const AudioParameters& params,
      const std::string& device_id,
      const LogCallback& log_callback) override;

  std::string GetDefaultInputDeviceID() override;
  std::string GetDefaultOutputDeviceID() override;

  // Used to track destruction of input and output streams.
  bool MaybeChangeBufferSize(AudioDeviceID device_id,
                             AudioUnit audio_unit,
                             AudioUnitElement element,
                             size_t desired_buffer_size) override;

  // Implementation of AudioManagerApple
  // Handle device capability ambient noise reduction. Currently, iOS is not
  // supported.
  bool DeviceSupportsAmbientNoiseReduction(AudioDeviceID device_id) override;
  bool SuppressNoiseReduction(AudioDeviceID device_id) override;
  void UnsuppressNoiseReduction(AudioDeviceID device_id) override;

  // Returns the maximum microphone analog volume or 0.0 if device does not
  // have volume control.
  double GetMaxInputVolume(AudioDeviceID device_id) override;

  // Sets the microphone analog volume, with range [0.0, 1.0] inclusive.
  double GetInputVolume(AudioDeviceID device_id) override;

  // Returns the microphone analog volume, with range [0.0, 1.0] inclusive.
  void SetInputVolume(AudioDeviceID device_id, double volume) override;

  // Returns the current muting state for the microphone.
  bool IsInputMuted(AudioDeviceID device_id) override;

  // Check if delayed start for stream is needed.
  bool ShouldDeferStreamStart() const override;

  // Retrieves the current hardware sample rate associated with a specified
  // device.
  int HardwareSampleRateForDevice(AudioDeviceID device_id) override;

  // If successful, this function returns no error and populates the out
  // parameter `input_format` with a valid ASBD. Otherwise, an error status code
  // will be returned.
  OSStatus GetInputDeviceStreamFormat(
      AudioUnit audio_unit,
      AudioStreamBasicDescription* input_format) override;

  // Hardware information
  double HardwareIOBufferDuration();
  double HardwareLatency(bool is_input);
  long GetDeviceChannels(bool is_input);

  // Gain
  bool IsInputGainSettable();

 protected:
  AudioParameters GetPreferredOutputStreamParameters(
      const std::string& output_device_id,
      const AudioParameters& input_params) override;
};

}  // namespace media

#endif  // MEDIA_AUDIO_IOS_AUDIO_MANAGER_IOS_H_

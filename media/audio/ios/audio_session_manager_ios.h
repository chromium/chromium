// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_IOS_AUDIO_SESSION_MANAGER_IOS_H_
#define MEDIA_AUDIO_IOS_AUDIO_SESSION_MANAGER_IOS_H_

#include "base/no_destructor.h"
#include "media/audio/audio_device_name.h"

namespace media {

// This utility class serves as a bridge to convey iOS platform-specific details
// to the AudioManagerIOS. Internally, it relies on iOS platform-specific
// singleton classes such as AVAudioSession and AVAudioApplication.
class AudioSessionManagerIOS {
 public:
  static AudioSessionManagerIOS& GetInstance();

  AudioSessionManagerIOS(const AudioSessionManagerIOS&) = delete;
  AudioSessionManagerIOS& operator=(const AudioSessionManagerIOS&) = delete;

  // Activate and Deactivate AVAudioSession.
  void SetActive(bool active);

  // Methods to support AudioManagerIOS
  bool HasAudioHardware(bool is_input);
  void GetAudioDeviceInfo(bool is_input, media::AudioDeviceNames* device_names);
  std::string GetDefaultOutputDeviceID();
  std::string GetDefaultInputDeviceID();

  // Hardware information
  double HardwareSampleRate();
  double HardwareIOBufferDuration();
  double HardwareLatency(bool is_input);
  long GetDeviceChannels(bool is_input);

  // Gain
  float GetInputGain();
  bool SetInputGain(float volume);
  bool IsInputMuted();
  bool IsInputGainSettable();

 private:
  friend base::NoDestructor<AudioSessionManagerIOS>;

  AudioSessionManagerIOS();
  ~AudioSessionManagerIOS() = default;

  void GetAudioInputDeviceInfo(media::AudioDeviceNames* device_names);
  void GetAudioOutputDeviceInfo(media::AudioDeviceNames* device_names);
};

}  // namespace media

#endif  // MEDIA_AUDIO_IOS_AUDIO_SESSION_MANAGER_IOS_H_

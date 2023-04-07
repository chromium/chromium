// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_IOS_AUDIO_SESSION_MANAGER_IOS_H_
#define MEDIA_AUDIO_IOS_AUDIO_SESSION_MANAGER_IOS_H_

#include "media/audio/audio_device_name.h"

namespace media {

class AudioSessionManagerIOS {
 public:
  AudioSessionManagerIOS(const AudioSessionManagerIOS&) = delete;
  AudioSessionManagerIOS& operator=(const AudioSessionManagerIOS&) = delete;

  ~AudioSessionManagerIOS() = default;
  AudioSessionManagerIOS();

  // Methods to support AudioManagerIOS
  bool HasAudioHardware(bool is_input);
  void GetAudioDeviceInfo(bool is_input, media::AudioDeviceNames* device_names);
  std::string GetDefaultOutputDeviceID();
  std::string GetDefaultInputDeviceID();
  int HardwareSampleRate();

 private:
  void GetAudioInputDeviceInfo(media::AudioDeviceNames* device_names);
  void GetAudioOutputDeviceInfo(media::AudioDeviceNames* device_names);
};

}  // namespace media

#endif  // MEDIA_AUDIO_IOS_AUDIO_SESSION_MANAGER_IOS_H_

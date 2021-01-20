// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_DEVICE_INFO_ACCESSOR_FOR_TESTS_H_
#define MEDIA_AUDIO_AUDIO_DEVICE_INFO_ACCESSOR_FOR_TESTS_H_

#include <string>

#include "media/audio/audio_device_description.h"
#include "media/base/audio_parameters.h"

namespace media {

class AudioManager;

// Accessor for protected device info-related AudioManager. To be used in media
// unit tests only.
class AudioDeviceInfoAccessorForTests {
 public:
  explicit AudioDeviceInfoAccessorForTests(AudioManager* audio_manager);

  bool HasAudioOutputDevices();

  bool HasAudioInputDevices();

  void GetAudioInputDeviceDescriptions(
      AudioDeviceDescriptions* device_descriptions);

  void GetAudioOutputDeviceDescriptions(
      AudioDeviceDescriptions* device_descriptions);

  AudioParameters GetDefaultOutputStreamParameters();

  AudioParameters GetOutputStreamParameters(const std::string& device_id);

  AudioParameters GetInputStreamParameters(const std::string& device_id);

  std::string GetAssociatedOutputDeviceID(const std::string& input_device_id);

  std::string GetDefaultInputDeviceID();

  std::string GetDefaultOutputDeviceID();

  std::string GetCommunicationsInputDeviceID();

  std::string GetCommunicationsOutputDeviceID();

 private:
  AudioManager* const audio_manager_;

  DISALLOW_COPY_AND_ASSIGN(AudioDeviceInfoAccessorForTests);
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_DEVICE_INFO_ACCESSOR_FOR_TESTS_H_

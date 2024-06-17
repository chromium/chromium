// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_AUDIO_MANAGER_POWER_USER_H_
#define SERVICES_AUDIO_AUDIO_MANAGER_POWER_USER_H_

#include <string>
#include "media/audio/audio_manager.h"
#include "media/base/audio_parameters.h"

namespace media {
class AudioManager;
}  // namespace media

namespace audio {

// Helper class to get access to the protected AudioManager API.
// TODO(crbug.com/40572543): Replace this class with a public API
// once the audio manager is inaccessible from outside the audio service.
class AudioManagerPowerUser {
 public:
  explicit AudioManagerPowerUser(media::AudioManager* audio_manager)
      : audio_manager_(audio_manager) {}

  std::string GetDefaultOutputDeviceID() {
    return audio_manager_->GetDefaultOutputDeviceID();
  }

  std::string GetCommunicationsOutputDeviceID() {
    return audio_manager_->GetCommunicationsOutputDeviceID();
  }

  media::AudioParameters GetOutputStreamParameters(
      const std::string& device_id) {
    std::string effective_device_id =
        media::AudioDeviceDescription::IsDefaultDevice(device_id)
            ? audio_manager_->GetDefaultOutputDeviceID()
            : device_id;

    return audio_manager_->GetOutputStreamParameters(effective_device_id);
  }

  media::AudioParameters GetInputStreamParameters(
      const std::string& device_id) {
    return audio_manager_->GetInputStreamParameters(device_id);
  }

 private:
  const raw_ptr<media::AudioManager> audio_manager_;
};

}  // namespace audio

#endif  // SERVICES_AUDIO_AUDIO_MANAGER_POWER_USER_H_

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_SYSTEM_HELPER_H_
#define MEDIA_AUDIO_AUDIO_SYSTEM_HELPER_H_

#include "media/audio/audio_system.h"
#include "media/base/media_export.h"

namespace media {
class AudioManager;

// Helper class wrapping AudioManager functionality. Methods to be called on
// audio thread only. Only audio system implementations are allowed to use it.
// See AudioSystem interface for method descriptions.
class MEDIA_EXPORT AudioSystemHelper {
 public:
  AudioSystemHelper(AudioManager* audio_manager);
  ~AudioSystemHelper();

  void GetInputStreamParameters(
      const std::string& device_id,
      AudioSystem::OnAudioParamsCallback on_params_cb);

  void GetOutputStreamParameters(
      const std::string& device_id,
      AudioSystem::OnAudioParamsCallback on_params_cb);

  void HasInputDevices(AudioSystem::OnBoolCallback on_has_devices_cb);

  void HasOutputDevices(AudioSystem::OnBoolCallback on_has_devices_cb);

  void GetDeviceDescriptions(
      bool for_input,
      AudioSystem::OnDeviceDescriptionsCallback on_descriptions_cp);

  void GetAssociatedOutputDeviceID(
      const std::string& input_device_id,
      AudioSystem::OnDeviceIdCallback on_device_id_cb);

  void GetInputDeviceInfo(
      const std::string& input_device_id,
      AudioSystem::OnInputDeviceInfoCallback on_input_device_info_cb);

 private:
  base::Optional<AudioParameters> ComputeInputParameters(
      const std::string& device_id);
  base::Optional<AudioParameters> ComputeOutputParameters(
      const std::string& device_id);

  AudioManager* const audio_manager_;

  DISALLOW_COPY_AND_ASSIGN(AudioSystemHelper);
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_SYSTEM_HELPER_H_

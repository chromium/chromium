// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_SYSTEM_IMPL_H_
#define MEDIA_AUDIO_AUDIO_SYSTEM_IMPL_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/threading/thread_checker.h"
#include "media/audio/audio_system.h"
#include "media/audio/audio_system_helper.h"

namespace media {
class AudioManager;

class MEDIA_EXPORT AudioSystemImpl : public AudioSystem {
 public:
  // Creates AudioSystem using the global AudioManager instance, which must be
  // created prior to that.
  static std::unique_ptr<AudioSystem> CreateInstance();

  explicit AudioSystemImpl(AudioManager* audio_manager);

  AudioSystemImpl(const AudioSystemImpl&) = delete;
  AudioSystemImpl& operator=(const AudioSystemImpl&) = delete;

  // AudioSystem implementation.
  void GetInputStreamParameters(const std::string& device_id,
                                OnAudioParamsCallback on_params_cb) override;

  void GetOutputStreamParameters(const std::string& device_id,
                                 OnAudioParamsCallback on_params_cb) override;

  void HasInputDevices(OnBoolCallback on_has_devices_cb) override;

  void HasOutputDevices(OnBoolCallback on_has_devices_cb) override;

  void GetDeviceDescriptions(
      bool for_input,
      OnDeviceDescriptionsCallback on_descriptions_cp) override;

  void GetAssociatedOutputDeviceID(const std::string& input_device_id,
                                   OnDeviceIdCallback on_device_id_cb) override;

  void GetInputDeviceInfo(
      const std::string& input_device_id,
      OnInputDeviceInfoCallback on_input_device_info_cb) override;

 private:
  // No-op if called on helper_.GetTaskRunner() thread, otherwise binds
  // |callback| to the current loop.
  template <typename... Args>
  base::OnceCallback<void(Args...)> MaybeBindToCurrentLoop(
      base::OnceCallback<void(Args...)> callback);

  THREAD_CHECKER(thread_checker_);
  const raw_ptr<AudioManager> audio_manager_;
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_SYSTEM_IMPL_H_

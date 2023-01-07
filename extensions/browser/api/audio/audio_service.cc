// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/audio/audio_service.h"

namespace extensions {

class AudioServiceImpl : public AudioService {
 public:
  AudioServiceImpl() = default;
  ~AudioServiceImpl() override = default;

  // Called by listeners to this service to add/remove themselves as observers.
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  // Start to query audio device information.
  void GetDevices(
      const api::audio::DeviceFilter* filter,
      base::OnceCallback<void(bool, DeviceInfoList)> callback) override;
  void SetActiveDeviceLists(const DeviceIdList* input_devices,
                            const DeviceIdList* output_devives,
                            base::OnceCallback<void(bool)> callback) override;
  void SetDeviceSoundLevel(const std::string& device_id,
                           int level_value,
                           base::OnceCallback<void(bool)> callback) override;
  void SetMute(bool is_input,
               bool value,
               base::OnceCallback<void(bool)> callback) override;
  void GetMute(bool is_input,
               base::OnceCallback<void(bool, bool)> callback) override;
};

void AudioServiceImpl::AddObserver(Observer* observer) {
  // Not implemented for platforms other than Chrome OS.
}

void AudioServiceImpl::RemoveObserver(Observer* observer) {
  // Not implemented for platforms other than Chrome OS.
}

AudioService::Ptr AudioService::CreateInstance(
    AudioDeviceIdCalculator* id_calculator) {
  return std::make_unique<AudioServiceImpl>();
}

void AudioServiceImpl::GetDevices(
    const api::audio::DeviceFilter* filter,
    base::OnceCallback<void(bool, DeviceInfoList)> callback) {
  DeviceInfoList devices_empty;
  std::move(callback).Run(false, std::move(devices_empty));
}

void AudioServiceImpl::SetActiveDeviceLists(
    const DeviceIdList* input_devices,
    const DeviceIdList* output_devives,
    base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(false);
}

void AudioServiceImpl::SetDeviceSoundLevel(
    const std::string& device_id,
    int level_value,
    base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(false);
}

void AudioServiceImpl::SetMute(bool is_input,
                               bool value,
                               base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(false);
}

void AudioServiceImpl::GetMute(bool is_input,
                               base::OnceCallback<void(bool, bool)> callback) {
  std::move(callback).Run(false, false);
}

}  // namespace extensions

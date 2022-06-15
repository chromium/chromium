// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/audio/audio_service.h"

namespace extensions {

class AudioServiceImpl : public AudioService {
 public:
  AudioServiceImpl() {}
  ~AudioServiceImpl() override {}

  // Called by listeners to this service to add/remove themselves as observers.
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  // Start to query audio device information.
  bool GetDevices(const api::audio::DeviceFilter* filter,
                  DeviceInfoList* devices_out) override;
  bool GetDevices(
      const api::audio::DeviceFilter* filter,
      base::OnceCallback<void(bool, DeviceInfoList)> callback) override;
  bool SetActiveDeviceLists(const DeviceIdList* input_devices,
                            const DeviceIdList* output_devives) override;
  bool SetActiveDeviceLists(const DeviceIdList* input_devices,
                            const DeviceIdList* output_devives,
                            base::OnceCallback<void(bool)> callback) override;
  bool SetDeviceSoundLevel(const std::string& device_id,
                           int level_value) override;
  bool SetDeviceSoundLevel(const std::string& device_id,
                           int level_value,
                           base::OnceCallback<void(bool)> callback) override;
  bool SetMute(bool is_input, bool value) override;
  bool SetMute(bool is_input,
               bool value,
               base::OnceCallback<void(bool)> callback) override;
  bool GetMute(bool is_input, bool* value) override;
  bool GetMute(bool is_input,
               base::OnceCallback<void(bool, bool)> callback) override;
};

void AudioServiceImpl::AddObserver(Observer* observer) {
  // TODO: implement this for platforms other than Chrome OS.
}

void AudioServiceImpl::RemoveObserver(Observer* observer) {
  // TODO: implement this for platforms other than Chrome OS.
}

AudioService::Ptr AudioService::CreateInstance(
    AudioDeviceIdCalculator* id_calculator) {
  return std::make_unique<AudioServiceImpl>();
}

bool AudioServiceImpl::GetDevices(const api::audio::DeviceFilter* filter,
                                  DeviceInfoList* devices_out) {
  return false;
}

bool AudioServiceImpl::GetDevices(
    const api::audio::DeviceFilter* filter,
    base::OnceCallback<void(bool, DeviceInfoList)> callback) {
  return false;
}

bool AudioServiceImpl::SetActiveDeviceLists(
    const DeviceIdList* input_devices,
    const DeviceIdList* output_devives) {
  return false;
}

bool AudioServiceImpl::SetActiveDeviceLists(
    const DeviceIdList* input_devices,
    const DeviceIdList* output_devives,
    base::OnceCallback<void(bool)> callback) {
  return false;
}

bool AudioServiceImpl::SetDeviceSoundLevel(const std::string& device_id,
                                           int level_value) {
  return false;
}

bool AudioServiceImpl::SetDeviceSoundLevel(
    const std::string& device_id,
    int level_value,
    base::OnceCallback<void(bool)> callback) {
  return false;
}

bool AudioServiceImpl::SetMute(bool is_input, bool value) {
  return false;
}

bool AudioServiceImpl::SetMute(bool is_input,
                               bool value,
                               base::OnceCallback<void(bool)> callback) {
  return false;
}

bool AudioServiceImpl::GetMute(bool is_input, bool* value) {
  return false;
}

bool AudioServiceImpl::GetMute(bool is_input,
                               base::OnceCallback<void(bool, bool)> callback) {
  return false;
}

}  // namespace extensions

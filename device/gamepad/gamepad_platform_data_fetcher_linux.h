// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_GAMEPAD_PLATFORM_DATA_FETCHER_LINUX_H_
#define DEVICE_GAMEPAD_GAMEPAD_PLATFORM_DATA_FETCHER_LINUX_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "device/gamepad/gamepad_data_fetcher.h"
#include "device/gamepad/gamepad_device_linux.h"
#include "device/gamepad/public/cpp/gamepads.h"
#include "device/gamepad/udev_gamepad_linux.h"

extern "C" {
struct udev_device;
}

namespace device {
class UdevLinux;
}

namespace device {

class DEVICE_GAMEPAD_EXPORT GamepadPlatformDataFetcherLinux
    : public GamepadDataFetcher {
 public:
  using Factory = GamepadDataFetcherFactoryImpl<GamepadPlatformDataFetcherLinux,
                                                GAMEPAD_SOURCE_LINUX_UDEV>;

  GamepadPlatformDataFetcherLinux();
  ~GamepadPlatformDataFetcherLinux() override;

  GamepadSource source() override;

  // GamepadDataFetcher implementation.
  void GetGamepadData(bool devices_changed_hint) override;

  void PlayEffect(
      int pad_index,
      mojom::GamepadHapticEffectType,
      mojom::GamepadEffectParametersPtr,
      mojom::GamepadHapticsManager::PlayVibrationEffectOnceCallback) override;

  void ResetVibration(
      int pad_index,
      mojom::GamepadHapticsManager::ResetVibrationActuatorCallback) override;

 private:
  // Updates the ID and mapper strings in |pad| with new device info.
  static void UpdateGamepadStrings(const std::string& name,
                                   uint16_t vendor_id,
                                   uint16_t product_id,
                                   bool has_standard_mapping,
                                   Gamepad* pad);

  void OnAddedToProvider() override;

  void RefreshDevice(udev_device* dev);
  void RefreshJoydevDevice(udev_device* dev, const UdevGamepadLinux& pad_info);
  void RefreshEvdevDevice(udev_device* dev, const UdevGamepadLinux& pad_info);
  void RefreshHidrawDevice(udev_device* dev, const UdevGamepadLinux& pad_info);
  void EnumerateSubsystemDevices(const std::string& subsystem);
  void ReadDeviceData(size_t index);

  GamepadDeviceLinux* GetDeviceWithJoydevIndex(int joydev_index);
  GamepadDeviceLinux* GetOrCreateMatchingDevice(
      const UdevGamepadLinux& pad_info);
  void RemoveDevice(GamepadDeviceLinux* device);

  std::unordered_set<std::unique_ptr<GamepadDeviceLinux>> devices_;

  std::unique_ptr<device::UdevLinux> udev_;

  DISALLOW_COPY_AND_ASSIGN(GamepadPlatformDataFetcherLinux);
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_GAMEPAD_PLATFORM_DATA_FETCHER_LINUX_H_

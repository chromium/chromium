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
#include "device/udev_linux/udev_watcher.h"

extern "C" {
struct udev_device;
}

namespace device {

class DEVICE_GAMEPAD_EXPORT GamepadPlatformDataFetcherLinux
    : public GamepadDataFetcher,
      public UdevWatcher::Observer {
 public:
  class Factory : public GamepadDataFetcherFactory {
   public:
    Factory(scoped_refptr<base::SequencedTaskRunner> dbus_runner);
    ~Factory() override;
    std::unique_ptr<GamepadDataFetcher> CreateDataFetcher() override;
    GamepadSource source() override;
    static GamepadSource static_source();

   private:
    scoped_refptr<base::SequencedTaskRunner> dbus_runner_;
  };

  GamepadPlatformDataFetcherLinux(
      scoped_refptr<base::SequencedTaskRunner> dbus_runner);
  ~GamepadPlatformDataFetcherLinux() override;

  GamepadSource source() override;

  // GamepadDataFetcher implementation.
  void GetGamepadData(bool devices_changed_hint) override;
  bool DisconnectUnrecognizedGamepad(int source_id) override;

  void PlayEffect(int pad_index,
                  mojom::GamepadHapticEffectType,
                  mojom::GamepadEffectParametersPtr,
                  mojom::GamepadHapticsManager::PlayVibrationEffectOnceCallback,
                  scoped_refptr<base::SequencedTaskRunner>) override;

  void ResetVibration(
      int pad_index,
      mojom::GamepadHapticsManager::ResetVibrationActuatorCallback,
      scoped_refptr<base::SequencedTaskRunner>) override;

 private:
  void OnAddedToProvider() override;

  void RefreshDevice(udev_device* dev);
  void RefreshJoydevDevice(udev_device* dev, const UdevGamepadLinux& pad_info);
  void RefreshEvdevDevice(udev_device* dev, const UdevGamepadLinux& pad_info);
  void RefreshHidrawDevice(udev_device* dev, const UdevGamepadLinux& pad_info);
  void ReadDeviceData(size_t index);

  void OnHidrawDeviceOpened(GamepadDeviceLinux* device);

  GamepadDeviceLinux* GetDeviceWithJoydevIndex(int joydev_index);
  GamepadDeviceLinux* GetOrCreateMatchingDevice(
      const UdevGamepadLinux& pad_info);
  void RemoveDevice(GamepadDeviceLinux* device);

  // UdevWatcher::Observer overrides
  void OnDeviceAdded(ScopedUdevDevicePtr device) override;
  void OnDeviceRemoved(ScopedUdevDevicePtr device) override;
  void OnDeviceChanged(ScopedUdevDevicePtr device) override;

  std::unordered_set<std::unique_ptr<GamepadDeviceLinux>> devices_;

  std::unique_ptr<device::UdevWatcher> udev_watcher_;

  scoped_refptr<base::SequencedTaskRunner> dbus_runner_;

  base::WeakPtrFactory<GamepadPlatformDataFetcherLinux> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(GamepadPlatformDataFetcherLinux);
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_GAMEPAD_PLATFORM_DATA_FETCHER_LINUX_H_

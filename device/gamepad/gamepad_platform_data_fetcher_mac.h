// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_GAMEPAD_PLATFORM_DATA_FETCHER_MAC_H_
#define DEVICE_GAMEPAD_GAMEPAD_PLATFORM_DATA_FETCHER_MAC_H_

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDManager.h>
#include <stddef.h>

#include <memory>

#include "base/mac/scoped_cftyperef.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "device/gamepad/gamepad_data_fetcher.h"
#include "device/gamepad/public/cpp/gamepad.h"
#include "device/gamepad/public/mojom/gamepad.mojom-forward.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace device {

class GamepadDeviceMac;

class GamepadPlatformDataFetcherMac : public GamepadDataFetcher {
 public:
  using Factory = GamepadDataFetcherFactoryImpl<GamepadPlatformDataFetcherMac,
                                                GAMEPAD_SOURCE_MAC_HID>;

  GamepadPlatformDataFetcherMac();
  ~GamepadPlatformDataFetcherMac() override;

  // GamepadDataFetcher public implementation.
  GamepadSource source() override;
  void GetGamepadData(bool devices_changed_hint) override;
  void PauseHint(bool paused) override;
  void PlayEffect(int source_id,
                  mojom::GamepadHapticEffectType,
                  mojom::GamepadEffectParametersPtr,
                  mojom::GamepadHapticsManager::PlayVibrationEffectOnceCallback,
                  scoped_refptr<base::SequencedTaskRunner>) override;
  void ResetVibration(
      int source_id,
      mojom::GamepadHapticsManager::ResetVibrationActuatorCallback,
      scoped_refptr<base::SequencedTaskRunner>) override;

 private:
  static GamepadPlatformDataFetcherMac* InstanceFromContext(void* context);
  static void DeviceAddCallback(void* context,
                                IOReturn result,
                                void* sender,
                                IOHIDDeviceRef ref);
  static void DeviceRemoveCallback(void* context,
                                   IOReturn result,
                                   void* sender,
                                   IOHIDDeviceRef ref);
  static void ValueChangedCallback(void* context,
                                   IOReturn result,
                                   void* sender,
                                   IOHIDValueRef ref);

  // GamepadDataFetcher private implementation.
  void OnAddedToProvider() override;

  // Returns the GamepadDeviceMac from |devices_| that has the given device
  // reference. Returns nullptr if the device is not in |devices_|.
  GamepadDeviceMac* GetGamepadFromHidDevice(IOHIDDeviceRef device);

  // Query device info for |device| and add it to |devices_| if it is a
  // gamepad.
  void DeviceAdd(IOHIDDeviceRef device);

  // Remove |device| from the set of connected devices.
  void DeviceRemove(IOHIDDeviceRef device);

  // Update the gamepad state for the button or axis referred to by |value|.
  void ValueChanged(IOHIDValueRef value);

  // Register for connection events and value change notifications for HID
  // devices.
  void RegisterForNotifications();

  // Unregister from connection events and value change notifications.
  void UnregisterFromNotifications();

  bool DisconnectUnrecognizedGamepad(int source_id) override;

  bool enabled_ = false;
  bool paused_ = false;
  base::ScopedCFTypeRef<IOHIDManagerRef> hid_manager_ref_;

  // A map of all devices using this data fetcher with the source_id as the key.
  std::unordered_map<int, std::unique_ptr<GamepadDeviceMac>> devices_;

  DISALLOW_COPY_AND_ASSIGN(GamepadPlatformDataFetcherMac);
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_GAMEPAD_PLATFORM_DATA_FETCHER_MAC_H_

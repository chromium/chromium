// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_NINTENDO_DATA_FETCHER_H_
#define DEVICE_GAMEPAD_NINTENDO_DATA_FETCHER_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "device/gamepad/gamepad_data_fetcher.h"
#include "device/gamepad/nintendo_controller.h"
#include "device/gamepad/public/cpp/gamepads.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/hid.mojom.h"

namespace device {
// Nintendo controllers are not typical HID gamepads and cannot be easily
// supported through the platform data fetchers. However, when they are HID
// devices we can use the HID backend to enumerate and initialize them.
//
// NintendoDataFetcher currently supports only Nintendo Switch devices:
// * Switch Joy-Con L
// * Switch Joy-Con R
//   - A pair of matching Joy-Cons may be associated to form a single gamepad.
// * Switch Pro Controller
//   - Supports USB and Bluetooth.
// * Switch Joy-Con Charging Grip
//   - Connects over USB and charges Joy-Cons.
//   - Can hold a pair of matching Joy-Cons to form a single gamepad.
//   - One or both Joy-Cons may be disconnected.
//
// When multiple Joy-Cons are connected, they are automatically associated to
// form a composite gamepad if:
// * The Joy-Cons form a matching pair (Joy-Con L and Joy-Con R).
// * They are connected using the same bus type (both Bluetooth or both USB).
// * Neither Joy-Con is already part of a composite gamepad.
// The composite gamepad functions identically to a Switch Pro Controller and
// exposes the same vendor and device ID but a different product name.
class DEVICE_GAMEPAD_EXPORT NintendoDataFetcher : public GamepadDataFetcher,
                                                  mojom::HidManagerClient {
 public:
  using Factory = GamepadDataFetcherFactoryImpl<NintendoDataFetcher,
                                                GAMEPAD_SOURCE_NINTENDO>;
  using ControllerMap =
      std::unordered_map<int, std::unique_ptr<NintendoController>>;

  NintendoDataFetcher();
  ~NintendoDataFetcher() override;

  // Add the newly-connected HID device described by |device_info|. Returns
  // true if the device was added successfully.
  bool AddDevice(mojom::HidDeviceInfoPtr device_info);

  // Remove the HID device with GUID matching |guid|. If the device is part of
  // a composite Switch device, the composite device will be decomposed and the
  // remaining subdevice will be returned to |switch_devices_|. Returns true if
  // the device was removed.
  bool RemoveDevice(const std::string& guid);

  // GamepadDataFetcher implementation.
  GamepadSource source() override;
  void GetGamepadData(bool devices_changed_hint) override;
  void PlayEffect(
      int source_id,
      mojom::GamepadHapticEffectType type,
      mojom::GamepadEffectParametersPtr params,
      mojom::GamepadHapticsManager::PlayVibrationEffectOnceCallback callback,
      scoped_refptr<base::SequencedTaskRunner> callback_runner) override;
  void ResetVibration(
      int source_id,
      mojom::GamepadHapticsManager::ResetVibrationActuatorCallback callback,
      scoped_refptr<base::SequencedTaskRunner> callback_runner) override;

  const ControllerMap& GetControllersForTesting() const { return controllers_; }

 private:
  // GamepadDataFetcher implementation.
  void OnAddedToProvider() override;

  // mojom::HidManagerClient implementation.
  void DeviceAdded(mojom::HidDeviceInfoPtr device_info) override;
  void DeviceRemoved(mojom::HidDeviceInfoPtr device_info) override;

  // mojom::HidManagerClient::GetDevicesAndSetClient callback.
  void OnGetDevices(std::vector<mojom::HidDeviceInfoPtr> device_infos);

  // Return a pointer to the controller with HID device GUID matching |guid|, or
  // nullptr if there is no match.
  NintendoController* GetControllerFromGuid(const std::string& guid);

  // Return a pointer to the controller with source ID |source_id|, or nullptr
  // if there is no match.
  NintendoController* GetControllerFromSourceId(int source_id);

  // Check |switch_devices_| for an unassociated device to associate with
  // |device|. If a valid device is found, it is removed from |switch_devices_|
  // and returned to the caller.
  std::unique_ptr<NintendoController> ExtractAssociatedDevice(
      const NintendoController* device);

  // Called when the gamepad with id |source_id| is ready to be exposed.
  void OnDeviceReady(int source_id);

  int next_source_id_ = 0;

  // A mapping from source ID to connected Nintendo Switch devices.
  ControllerMap controllers_;

  mojo::Remote<mojom::HidManager> hid_manager_;
  mojo::AssociatedReceiver<mojom::HidManagerClient> receiver_{this};
  base::WeakPtrFactory<NintendoDataFetcher> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(NintendoDataFetcher);
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_NINTENDO_DATA_FETCHER_H_

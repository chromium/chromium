// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_BLE_FIDO_BLE_PAIRING_DELEGATE_H_
#define DEVICE_FIDO_BLE_FIDO_BLE_PAIRING_DELEGATE_H_

#include <stdint.h>

#include <string>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "device/bluetooth/bluetooth_device.h"

namespace device {

// Handles incoming Bluetooth pairing events. Namely,
//   a) Caching BLE Pin code when user inputs the pin code.
//   b) Handling logic of BluetoothDevice::RequestPinCode() and
//      BluetoothDevice::RequestPasskey().
// All other events are either silenced or BluetoothDevice::CancelPairing() is
// invoked.
class COMPONENT_EXPORT(DEVICE_FIDO) FidoBlePairingDelegate
    : public BluetoothDevice::PairingDelegate {
 public:
  FidoBlePairingDelegate();
  ~FidoBlePairingDelegate() override;

  // BluetoothDevice::PairingDelegate:
  void RequestPinCode(BluetoothDevice* device) override;
  void RequestPasskey(BluetoothDevice* device) override;
  void DisplayPinCode(BluetoothDevice* device,
                      const std::string& pincode) override;
  void DisplayPasskey(BluetoothDevice* device, uint32_t passkey) override;
  void KeysEntered(BluetoothDevice* device, uint32_t entered) override;
  void ConfirmPasskey(BluetoothDevice* device, uint32_t passkey) override;
  void AuthorizePairing(BluetoothDevice* device) override;

  void StoreBlePinCodeForDevice(std::string device_address,
                                std::string pin_code);
  void ChangeStoredDeviceAddress(const std::string& old_address,
                                 std::string new_address);
  void CancelPairingOnAllKnownDevices(BluetoothAdapter* adapter);

 private:
  friend class FidoBlePairingDelegateTest;
  friend class FidoBleAdapterManagerTest;

  base::flat_map<std::string, std::string> bluetooth_device_pincode_map_;

  DISALLOW_COPY_AND_ASSIGN(FidoBlePairingDelegate);
};

}  // namespace device

#endif  // DEVICE_FIDO_BLE_FIDO_BLE_PAIRING_DELEGATE_H_

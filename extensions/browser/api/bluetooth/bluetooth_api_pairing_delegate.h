// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_BLUETOOTH_BLUETOOTH_API_PAIRING_DELEGATE_H_
#define EXTENSIONS_BROWSER_API_BLUETOOTH_BLUETOOTH_API_PAIRING_DELEGATE_H_

#include <stdint.h>

#include <string>

#include "base/memory/raw_ptr.h"
#include "device/bluetooth/bluetooth_device.h"
#include "extensions/common/api/bluetooth_private.h"

namespace content {
class BrowserContext;
}

namespace extensions {

// A pairing delegate to dispatch incoming Bluetooth pairing events to the API
// event router.
class BluetoothApiPairingDelegate
    : public device::BluetoothDevice::PairingDelegate {
 public:
  explicit BluetoothApiPairingDelegate(
      content::BrowserContext* browser_context);

  BluetoothApiPairingDelegate(const BluetoothApiPairingDelegate&) = delete;
  BluetoothApiPairingDelegate& operator=(const BluetoothApiPairingDelegate&) =
      delete;

  ~BluetoothApiPairingDelegate() override;

  // device::PairingDelegate overrides:
  void RequestPinCode(device::BluetoothDevice* device) override;
  void RequestPasskey(device::BluetoothDevice* device) override;
  void DisplayPinCode(device::BluetoothDevice* device,
                      const std::string& pincode) override;
  void DisplayPasskey(device::BluetoothDevice* device,
                      uint32_t passkey) override;
  void KeysEntered(device::BluetoothDevice* device, uint32_t entered) override;
  void ConfirmPasskey(device::BluetoothDevice* device,
                      uint32_t passkey) override;
  void AuthorizePairing(device::BluetoothDevice* device) override;

 private:
  // Dispatches a pairing event to the extension.
  void DispatchPairingEvent(
      const api::bluetooth_private::PairingEvent& pairing_event);

  raw_ptr<content::BrowserContext> browser_context_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_BLUETOOTH_BLUETOOTH_API_PAIRING_DELEGATE_H_

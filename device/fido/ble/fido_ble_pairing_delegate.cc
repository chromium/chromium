// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/ble/fido_ble_pairing_delegate.h"

#include <utility>

#include "base/strings/string_number_conversions.h"
#include "device/fido/ble/fido_ble_device.h"

namespace device {

FidoBlePairingDelegate::FidoBlePairingDelegate() = default;

FidoBlePairingDelegate::~FidoBlePairingDelegate() = default;

void FidoBlePairingDelegate::RequestPinCode(device::BluetoothDevice* device) {
  auto it = bluetooth_device_pincode_map_.find(
      FidoBleDevice::GetIdForAddress(device->GetAddress()));
  if (it == bluetooth_device_pincode_map_.end()) {
    device->CancelPairing();
    return;
  }

  device->SetPinCode(it->second);
}

void FidoBlePairingDelegate::RequestPasskey(device::BluetoothDevice* device) {
  auto it = bluetooth_device_pincode_map_.find(
      FidoBleDevice::GetIdForAddress(device->GetAddress()));
  if (it == bluetooth_device_pincode_map_.end()) {
    device->CancelPairing();
    return;
  }

  uint32_t pass_key;
  if (!base::StringToUint(it->second, &pass_key)) {
    device->CancelPairing();
    return;
  }

  device->SetPasskey(pass_key);
}

void FidoBlePairingDelegate::DisplayPinCode(device::BluetoothDevice* device,
                                            const std::string& pincode) {
  NOTIMPLEMENTED();
}

void FidoBlePairingDelegate::DisplayPasskey(device::BluetoothDevice* device,
                                            uint32_t passkey) {
  NOTIMPLEMENTED();
}

void FidoBlePairingDelegate::KeysEntered(device::BluetoothDevice* device,
                                         uint32_t entered) {
  NOTIMPLEMENTED();
}

void FidoBlePairingDelegate::ConfirmPasskey(device::BluetoothDevice* device,
                                            uint32_t passkey) {
  NOTIMPLEMENTED();
  device->CancelPairing();
}

void FidoBlePairingDelegate::AuthorizePairing(device::BluetoothDevice* device) {
  NOTIMPLEMENTED();
  device->CancelPairing();
}

void FidoBlePairingDelegate::StoreBlePinCodeForDevice(
    std::string device_address,
    std::string pin_code) {
  bluetooth_device_pincode_map_.insert_or_assign(std::move(device_address),
                                                 std::move(pin_code));
}

void FidoBlePairingDelegate::ChangeStoredDeviceAddress(
    const std::string& old_address,
    std::string new_address) {
  auto it = bluetooth_device_pincode_map_.find(old_address);
  if (it != bluetooth_device_pincode_map_.end()) {
    std::string pincode = std::move(it->second);
    bluetooth_device_pincode_map_.erase(it);
    bluetooth_device_pincode_map_.insert_or_assign(std::move(new_address),
                                                   std::move(pincode));
  }
}

void FidoBlePairingDelegate::CancelPairingOnAllKnownDevices(
    BluetoothAdapter* adapter) {
  DCHECK(adapter);
  auto bluetooth_devices = adapter->GetDevices();
  for (const auto& may_be_paired_device_info : bluetooth_device_pincode_map_) {
    const auto& authenticator_id = may_be_paired_device_info.first;
    auto it = std::find_if(
        bluetooth_devices.begin(), bluetooth_devices.end(),
        [&authenticator_id](const auto* device) {
          return FidoBleDevice::GetIdForAddress(device->GetAddress()) ==
                 authenticator_id;
        });
    if (it == bluetooth_devices.end())
      continue;

    // TODO(hongjunchoi): Change this so that this is only invoked when we know
    // that WebAuthN request was in middle of pairing -- not unconditionally.
    // See: https://crbug.com/892697
    (*it)->CancelPairing();
  }
}

}  // namespace device

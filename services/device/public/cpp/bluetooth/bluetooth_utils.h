// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_BLUETOOTH_BLUETOOTH_UTILS_H_
#define SERVICES_DEVICE_PUBLIC_CPP_BLUETOOTH_BLUETOOTH_UTILS_H_

#include <string>

#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "services/device/public/mojom/bluetooth_system.mojom.h"

namespace device {

// Returns the address suitable for displaying e.g. "AA:BB:CC:DD:00:11".
std::u16string GetBluetoothAddressForDisplay(
    const std::array<uint8_t, 6>& address);

// Returns the name of the device suitable for displaying, this may
// be a synthesized string containing the address and localized type name
// if the device has no obtained name.
std::u16string GetBluetoothDeviceNameForDisplay(
    const mojom::BluetoothDeviceInfoPtr& device_info);

// Returns an accessibility label for the device based on name or address and
// device type.
std::u16string GetBluetoothDeviceLabelForAccessibility(
    const mojom::BluetoothDeviceInfoPtr& device_info);

// Returns a BluetoothUUID for a Bluetooth SPP device.
const BluetoothUUID& GetSerialPortProfileUUID();

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_BLUETOOTH_BLUETOOTH_UTILS_H_

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_BLUETOOTH_BLUETOOTH_UTILS_H_
#define SERVICES_DEVICE_PUBLIC_CPP_BLUETOOTH_BLUETOOTH_UTILS_H_

#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

namespace device {

// Returns a BluetoothUUID for a Bluetooth SPP device.
const BluetoothUUID& GetSerialPortProfileUUID();

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_BLUETOOTH_BLUETOOTH_UTILS_H_

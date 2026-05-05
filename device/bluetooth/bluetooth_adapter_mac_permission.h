// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_MAC_PERMISSION_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_MAC_PERMISSION_H_

#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_export.h"

namespace device {

// Returns the macOS Bluetooth permission status.
// This is a standalone function so that we can check permission without
// creating or initializing the Bluetooth adapter.
DEVICE_BLUETOOTH_EXPORT BluetoothAdapter::PermissionStatus
GetMacBluetoothPermissionStatus();

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_MAC_PERMISSION_H_

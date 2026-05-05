// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_adapter_mac_permission.h"

#import <CoreBluetooth/CoreBluetooth.h>

namespace device {

BluetoothAdapter::PermissionStatus GetMacBluetoothPermissionStatus() {
  switch (CBCentralManager.authorization) {
    case CBManagerAuthorizationNotDetermined:
      return BluetoothAdapter::PermissionStatus::kUndetermined;
    case CBManagerAuthorizationRestricted:
    case CBManagerAuthorizationDenied:
      return BluetoothAdapter::PermissionStatus::kDenied;
    case CBManagerAuthorizationAllowedAlways:
      return BluetoothAdapter::PermissionStatus::kAllowed;
  }
}

}  // namespace device

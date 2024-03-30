// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_DEVICE_MAC_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_DEVICE_MAC_H_

#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_gatt_service.h"

@class NSError;

namespace device {

class BluetoothAdapter;

class DEVICE_BLUETOOTH_EXPORT BluetoothDeviceMac : public BluetoothDevice {
 public:
  BluetoothDeviceMac(const BluetoothDeviceMac&) = delete;
  BluetoothDeviceMac& operator=(const BluetoothDeviceMac&) = delete;

  ~BluetoothDeviceMac() override;

  // Converts between ConnectErrorCode and NSError.
  static NSError* GetNSErrorFromConnectErrorCode(
      BluetoothDevice::ConnectErrorCode error_code);
  static BluetoothDevice::ConnectErrorCode GetConnectErrorCodeFromNSError(
      NSError* error);
  static NSError* GetNSErrorFromGattErrorCode(
      BluetoothGattService::GattErrorCode error_code);
  static BluetoothGattService::GattErrorCode GetGattErrorCodeFromNSError(
      NSError* error);

 protected:
  explicit BluetoothDeviceMac(BluetoothAdapter* adapter);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_DEVICE_MAC_H_

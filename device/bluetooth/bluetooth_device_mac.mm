// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_device_mac.h"

#import <Foundation/Foundation.h>

#include "device/bluetooth/bluetooth_adapter.h"

static NSString* const kConnectErrorDomain = @"ConnectErrorCode";
static NSString* const kGattErrorDomain = @"GattErrorCode";

namespace device {

BluetoothDeviceMac::BluetoothDeviceMac(BluetoothAdapter* adapter)
    : BluetoothDevice(adapter) {}

BluetoothDeviceMac::~BluetoothDeviceMac() = default;

NSError* BluetoothDeviceMac::GetNSErrorFromConnectErrorCode(
    BluetoothDevice::ConnectErrorCode error_code) {
  // TODO(http://crbug.com/585894): Need to convert the error.
  return [NSError errorWithDomain:kConnectErrorDomain
                             code:error_code
                         userInfo:nil];
}

BluetoothDevice::ConnectErrorCode
BluetoothDeviceMac::GetConnectErrorCodeFromNSError(NSError* error) {
  if ([error.domain isEqualToString:kConnectErrorDomain]) {
    BluetoothDevice::ConnectErrorCode connect_error_code =
        (BluetoothDevice::ConnectErrorCode)error.code;
    if (connect_error_code >= 0 ||
        connect_error_code < BluetoothDevice::NUM_CONNECT_ERROR_CODES) {
      return connect_error_code;
    }
    DCHECK(false);
    return BluetoothDevice::ERROR_FAILED;
  }
  // TODO(http://crbug.com/585894): Need to convert the error.
  return BluetoothDevice::ERROR_FAILED;
}

NSError* BluetoothDeviceMac::GetNSErrorFromGattErrorCode(
    BluetoothGattService::GattErrorCode error_code) {
  // TODO(http://crbug.com/619595): Need to convert the GattErrorCode value to
  // a CBError value.
  return [NSError errorWithDomain:kGattErrorDomain
                             code:static_cast<int>(error_code)
                         userInfo:nil];
}

BluetoothGattService::GattErrorCode
BluetoothDeviceMac::GetGattErrorCodeFromNSError(NSError* error) {
  if ([error.domain isEqualToString:kGattErrorDomain]) {
    BluetoothGattService::GattErrorCode gatt_error_code =
        (BluetoothGattService::GattErrorCode)error.code;
    if (static_cast<int>(gatt_error_code) >= 0 ||
        static_cast<int>(gatt_error_code) <=
            static_cast<int>(
                BluetoothGattService::GattErrorCode::kNotSupported)) {
      return gatt_error_code;
    }
    NOTREACHED();
  }
  // TODO(http://crbug.com/619595): Need to convert the error code from
  // CoreBluetooth to a GattErrorCode value.
  return BluetoothGattService::GattErrorCode::kFailed;
}

}  // namespace device

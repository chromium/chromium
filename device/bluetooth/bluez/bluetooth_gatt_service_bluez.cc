// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluez/bluetooth_gatt_service_bluez.h"

#include "base/check.h"
#include "device/bluetooth/bluetooth_gatt_service.h"
#include "device/bluetooth/bluez/bluetooth_adapter_bluez.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace bluez {

BluetoothGattServiceBlueZ::BluetoothGattServiceBlueZ(
    BluetoothAdapterBlueZ* adapter,
    dbus::ObjectPath object_path)
    : adapter_(adapter), object_path_(object_path) {
  DCHECK(adapter_);
}

BluetoothGattServiceBlueZ::~BluetoothGattServiceBlueZ() = default;

std::string BluetoothGattServiceBlueZ::GetIdentifier() const {
  return object_path_.value();
}

// static
device::BluetoothGattService::GattErrorCode
BluetoothGattServiceBlueZ::DBusErrorToServiceError(std::string error_name) {
  auto code = device::BluetoothGattService::GattErrorCode::kUnknown;
  if (error_name == bluetooth_gatt_service::kErrorFailed) {
    code = device::BluetoothGattService::GattErrorCode::kFailed;
  } else if (error_name == bluetooth_gatt_service::kErrorInProgress) {
    code = device::BluetoothGattService::GattErrorCode::kInProgress;
  } else if (error_name == bluetooth_gatt_service::kErrorInvalidValueLength) {
    code = device::BluetoothGattService::GattErrorCode::kInvalidLength;
  } else if (error_name == bluetooth_gatt_service::kErrorNotPermitted) {
    code = device::BluetoothGattService::GattErrorCode::kNotPermitted;
  } else if (error_name == bluetooth_gatt_service::kErrorNotAuthorized) {
    code = device::BluetoothGattService::GattErrorCode::kNotAuthorized;
  } else if (error_name == bluetooth_gatt_service::kErrorNotPaired) {
    code = device::BluetoothGattService::GattErrorCode::kNotPaired;
  } else if (error_name == bluetooth_gatt_service::kErrorNotSupported) {
    code = device::BluetoothGattService::GattErrorCode::kNotSupported;
  }
  return code;
}

BluetoothAdapterBlueZ* BluetoothGattServiceBlueZ::GetAdapter() const {
  return adapter_;
}

}  // namespace bluez

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/public/mojom/gatt_service_mojom_traits.h"

namespace mojo {

bluetooth::mojom::GattServiceErrorCode
EnumTraits<bluetooth::mojom::GattServiceErrorCode,
           device::BluetoothGattService::GattErrorCode>::
    ToMojom(device::BluetoothGattService::GattErrorCode input) {
  switch (input) {
    case device::BluetoothGattService::GattErrorCode::kUnknown:
      return bluetooth::mojom::GattServiceErrorCode::kUnknown;
    case device::BluetoothGattService::GattErrorCode::kFailed:
      return bluetooth::mojom::GattServiceErrorCode::kFailed;
    case device::BluetoothGattService::GattErrorCode::kInProgress:
      return bluetooth::mojom::GattServiceErrorCode::kInProgress;
    case device::BluetoothGattService::GattErrorCode::kInvalidLength:
      return bluetooth::mojom::GattServiceErrorCode::kInvalidLength;
    case device::BluetoothGattService::GattErrorCode::kNotPermitted:
      return bluetooth::mojom::GattServiceErrorCode::kNotPermitted;
    case device::BluetoothGattService::GattErrorCode::kNotAuthorized:
      return bluetooth::mojom::GattServiceErrorCode::kNotAuthorized;
    case device::BluetoothGattService::GattErrorCode::kNotPaired:
      return bluetooth::mojom::GattServiceErrorCode::kNotPaired;
    case device::BluetoothGattService::GattErrorCode::kNotSupported:
      return bluetooth::mojom::GattServiceErrorCode::kNotSupported;
  }
}

device::BluetoothGattService::GattErrorCode
EnumTraits<bluetooth::mojom::GattServiceErrorCode,
           device::BluetoothGattService::GattErrorCode>::
    FromMojom(bluetooth::mojom::GattServiceErrorCode input) {
  switch (input) {
    case bluetooth::mojom::GattServiceErrorCode::kUnknown:
      return device::BluetoothGattService::GattErrorCode::kUnknown;
    case bluetooth::mojom::GattServiceErrorCode::kFailed:
      return device::BluetoothGattService::GattErrorCode::kFailed;
    case bluetooth::mojom::GattServiceErrorCode::kInProgress:
      return device::BluetoothGattService::GattErrorCode::kInProgress;
    case bluetooth::mojom::GattServiceErrorCode::kInvalidLength:
      return device::BluetoothGattService::GattErrorCode::kInvalidLength;
    case bluetooth::mojom::GattServiceErrorCode::kNotPermitted:
      return device::BluetoothGattService::GattErrorCode::kNotPermitted;
    case bluetooth::mojom::GattServiceErrorCode::kNotAuthorized:
      return device::BluetoothGattService::GattErrorCode::kNotAuthorized;
    case bluetooth::mojom::GattServiceErrorCode::kNotPaired:
      return device::BluetoothGattService::GattErrorCode::kNotPaired;
    case bluetooth::mojom::GattServiceErrorCode::kNotSupported:
      return device::BluetoothGattService::GattErrorCode::kNotSupported;
  }
}

}  // namespace mojo

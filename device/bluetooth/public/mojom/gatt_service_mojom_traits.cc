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

bool EnumTraits<bluetooth::mojom::GattServiceErrorCode,
                device::BluetoothGattService::GattErrorCode>::
    FromMojom(bluetooth::mojom::GattServiceErrorCode input,
              device::BluetoothGattService::GattErrorCode* output) {
  switch (input) {
    case bluetooth::mojom::GattServiceErrorCode::kUnknown:
      *output = device::BluetoothGattService::GattErrorCode::kUnknown;
      return true;
    case bluetooth::mojom::GattServiceErrorCode::kFailed:
      *output = device::BluetoothGattService::GattErrorCode::kFailed;
      return true;
    case bluetooth::mojom::GattServiceErrorCode::kInProgress:
      *output = device::BluetoothGattService::GattErrorCode::kInProgress;
      return true;
    case bluetooth::mojom::GattServiceErrorCode::kInvalidLength:
      *output = device::BluetoothGattService::GattErrorCode::kInvalidLength;
      return true;
    case bluetooth::mojom::GattServiceErrorCode::kNotPermitted:
      *output = device::BluetoothGattService::GattErrorCode::kNotPermitted;
      return true;
    case bluetooth::mojom::GattServiceErrorCode::kNotAuthorized:
      *output = device::BluetoothGattService::GattErrorCode::kNotAuthorized;
      return true;
    case bluetooth::mojom::GattServiceErrorCode::kNotPaired:
      *output = device::BluetoothGattService::GattErrorCode::kNotPaired;
      return true;
    case bluetooth::mojom::GattServiceErrorCode::kNotSupported:
      *output = device::BluetoothGattService::GattErrorCode::kNotSupported;
      return true;
  }
}

}  // namespace mojo

// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_PUBLIC_MOJOM_CONNECT_RESULT_TYPE_CONVERTER_H_
#define DEVICE_BLUETOOTH_PUBLIC_MOJOM_CONNECT_RESULT_TYPE_CONVERTER_H_

#include "base/notreached.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/public/mojom/adapter.mojom.h"
#include "mojo/public/cpp/bindings/type_converter.h"

namespace mojo {

// TypeConverter to translate from
// device::BluetoothDevice::ConnectErrorCode to bluetooth.mojom.ConnectResult.
// TODO(crbug.com/40494280): Replace because TypeConverter is deprecated.
template <>
struct TypeConverter<bluetooth::mojom::ConnectResult,
                     device::BluetoothDevice::ConnectErrorCode> {
  static bluetooth::mojom::ConnectResult Convert(
      const device::BluetoothDevice::ConnectErrorCode& input) {
    switch (input) {
      case device::BluetoothDevice::ConnectErrorCode::ERROR_AUTH_CANCELED:
        return bluetooth::mojom::ConnectResult::AUTH_CANCELED;
      case device::BluetoothDevice::ConnectErrorCode::ERROR_AUTH_FAILED:
        return bluetooth::mojom::ConnectResult::AUTH_FAILED;
      case device::BluetoothDevice::ConnectErrorCode::ERROR_AUTH_REJECTED:
        return bluetooth::mojom::ConnectResult::AUTH_REJECTED;
      case device::BluetoothDevice::ConnectErrorCode::ERROR_AUTH_TIMEOUT:
        return bluetooth::mojom::ConnectResult::AUTH_TIMEOUT;
      case device::BluetoothDevice::ConnectErrorCode::ERROR_FAILED:
        return bluetooth::mojom::ConnectResult::FAILED;
      case device::BluetoothDevice::ConnectErrorCode::ERROR_INPROGRESS:
        return bluetooth::mojom::ConnectResult::INPROGRESS;
      case device::BluetoothDevice::ConnectErrorCode::ERROR_UNKNOWN:
        return bluetooth::mojom::ConnectResult::UNKNOWN;
      case device::BluetoothDevice::ConnectErrorCode::ERROR_UNSUPPORTED_DEVICE:
        return bluetooth::mojom::ConnectResult::UNSUPPORTED_DEVICE;
      case device::BluetoothDevice::ConnectErrorCode::ERROR_DEVICE_NOT_READY:
        return bluetooth::mojom::ConnectResult::NOT_READY;
      case device::BluetoothDevice::ConnectErrorCode::ERROR_ALREADY_CONNECTED:
        return bluetooth::mojom::ConnectResult::ALREADY_CONNECTED;
      case device::BluetoothDevice::ConnectErrorCode::
          ERROR_DEVICE_ALREADY_EXISTS:
        return bluetooth::mojom::ConnectResult::ALREADY_EXISTS;
      case device::BluetoothDevice::ConnectErrorCode::ERROR_DEVICE_UNCONNECTED:
        return bluetooth::mojom::ConnectResult::NOT_CONNECTED;
      case device::BluetoothDevice::ConnectErrorCode::ERROR_DOES_NOT_EXIST:
        return bluetooth::mojom::ConnectResult::DOES_NOT_EXIST;
      case device::BluetoothDevice::ConnectErrorCode::ERROR_INVALID_ARGS:
        return bluetooth::mojom::ConnectResult::INVALID_ARGS;
      case device::BluetoothDevice::ConnectErrorCode::ERROR_NON_AUTH_TIMEOUT:
        return bluetooth::mojom::ConnectResult::NON_AUTH_TIMEOUT;
      case device::BluetoothDevice::ConnectErrorCode::ERROR_NO_MEMORY:
        return bluetooth::mojom::ConnectResult::NO_MEMORY;
      case device::BluetoothDevice::ConnectErrorCode::ERROR_JNI_ENVIRONMENT:
        return bluetooth::mojom::ConnectResult::JNI_ENVIRONMENT;
      case device::BluetoothDevice::ConnectErrorCode::ERROR_JNI_THREAD_ATTACH:
        return bluetooth::mojom::ConnectResult::JNI_THREAD_ATTACH;
      case device::BluetoothDevice::ConnectErrorCode::ERROR_WAKELOCK:
        return bluetooth::mojom::ConnectResult::WAKELOCK;
      case device::BluetoothDevice::ConnectErrorCode::ERROR_UNEXPECTED_STATE:
        return bluetooth::mojom::ConnectResult::UNEXPECTED_STATE;
      case device::BluetoothDevice::ConnectErrorCode::ERROR_SOCKET:
        return bluetooth::mojom::ConnectResult::SOCKET;
      case device::BluetoothDevice::ConnectErrorCode::NUM_CONNECT_ERROR_CODES:
        NOTREACHED();
    }
    NOTREACHED();
  }
};
}  // namespace mojo

#endif  // DEVICE_BLUETOOTH_PUBLIC_MOJOM_CONNECT_RESULT_TYPE_CONVERTER_H_

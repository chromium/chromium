// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_PUBLIC_MOJOM_GATT_SERVICE_MOJOM_TRAITS_H_
#define DEVICE_BLUETOOTH_PUBLIC_MOJOM_GATT_SERVICE_MOJOM_TRAITS_H_

#include "device/bluetooth/bluetooth_gatt_service.h"
#include "device/bluetooth/public/mojom/gatt_service_error_code.mojom-shared.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct EnumTraits<bluetooth::mojom::GattServiceErrorCode,
                  device::BluetoothGattService::GattErrorCode> {
  static bluetooth::mojom::GattServiceErrorCode ToMojom(
      device::BluetoothGattService::GattErrorCode input);
  static bool FromMojom(bluetooth::mojom::GattServiceErrorCode input,
                        device::BluetoothGattService::GattErrorCode* output);
};

}  // namespace mojo

#endif  // DEVICE_BLUETOOTH_PUBLIC_MOJOM_GATT_SERVICE_MOJOM_TRAITS_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_PUBLIC_MOJOM_GATT_CHARACTERISTIC_MOJOM_TRAITS_H_
#define DEVICE_BLUETOOTH_PUBLIC_MOJOM_GATT_CHARACTERISTIC_MOJOM_TRAITS_H_

#include "device/bluetooth/bluetooth_local_gatt_characteristic.h"
#include "device/bluetooth/public/mojom/gatt_characteristic_permissions.mojom-shared.h"
#include "device/bluetooth/public/mojom/gatt_characteristic_properties.mojom-shared.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct StructTraits<bluetooth::mojom::GattCharacteristicPropertiesDataView,
                    device::BluetoothGattCharacteristic::Properties> {
  static bool broadcast(
      const device::BluetoothGattCharacteristic::Properties& input);
  static bool read(
      const device::BluetoothGattCharacteristic::Properties& input);
  static bool write_without_response(
      const device::BluetoothGattCharacteristic::Properties& input);
  static bool write(
      const device::BluetoothGattCharacteristic::Properties& input);
  static bool notify(
      const device::BluetoothGattCharacteristic::Properties& input);
  static bool indicate(
      const device::BluetoothGattCharacteristic::Properties& input);
  static bool authenticated_signed_writes(
      const device::BluetoothGattCharacteristic::Properties& input);
  static bool extended_properties(
      const device::BluetoothGattCharacteristic::Properties& input);
  static bool reliable_write(
      const device::BluetoothGattCharacteristic::Properties& input);
  static bool writable_auxiliaries(
      const device::BluetoothGattCharacteristic::Properties& input);
  static bool read_encrypted(
      const device::BluetoothGattCharacteristic::Properties& input);
  static bool write_encrypted(
      const device::BluetoothGattCharacteristic::Properties& input);
  static bool read_encrypted_authenticated(
      const device::BluetoothGattCharacteristic::Properties& input);
  static bool write_encrypted_authenticated(
      const device::BluetoothGattCharacteristic::Properties& input);

  static bool Read(bluetooth::mojom::GattCharacteristicPropertiesDataView input,
                   device::BluetoothGattCharacteristic::Properties* output);
};

template <>
struct StructTraits<bluetooth::mojom::GattCharacteristicPermissionsDataView,
                    device::BluetoothGattCharacteristic::Permissions> {
  static bool read(
      const device::BluetoothGattCharacteristic::Permissions& input);
  static bool write(
      const device::BluetoothGattCharacteristic::Permissions& input);
  static bool read_encrypted(
      const device::BluetoothGattCharacteristic::Permissions& input);
  static bool write_encrypted(
      const device::BluetoothGattCharacteristic::Permissions& input);
  static bool read_encrypted_authenticated(
      const device::BluetoothGattCharacteristic::Permissions& input);
  static bool write_encrypted_authenticated(
      const device::BluetoothGattCharacteristic::Permissions& input);

  static bool Read(
      bluetooth::mojom::GattCharacteristicPermissionsDataView input,
      device::BluetoothGattCharacteristic::Permissions* output);
};

}  // namespace mojo

#endif  // DEVICE_BLUETOOTH_PUBLIC_MOJOM_GATT_CHARACTERISTIC_MOJOM_TRAITS_H_

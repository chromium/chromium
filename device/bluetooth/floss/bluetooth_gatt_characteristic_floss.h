// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_GATT_CHARACTERISTIC_FLOSS_H_
#define DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_GATT_CHARACTERISTIC_FLOSS_H_

#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"

namespace floss {

class DEVICE_BLUETOOTH_EXPORT BluetoothGattCharacteristicFloss
    : public device::BluetoothGattCharacteristic {
 public:
  BluetoothGattCharacteristicFloss(const BluetoothGattCharacteristicFloss&) =
      delete;
  BluetoothGattCharacteristicFloss& operator=(
      const BluetoothGattCharacteristicFloss&) = delete;

  // Convert properties and permissions from Floss provided values to what
  // is expected by |device::bluetooth::BluetoothGattCharacteristics|. The Floss
  // representations are part of the core spec (table 3.5 in Core 5.3).
  //
  // Due to the way |device::bluetooth| has chosen to represent properties, the
  // values in permissions and properties are inter-related and must be updated
  // together.
  static std::pair<Properties, Permissions> ConvertPropsAndPermsFromFloss(
      const uint8_t properties,
      const uint16_t permissions);

  // Convert properties and permissions to the Floss representations.
  static std::pair<uint8_t, uint16_t> ConvertPropsAndPermsToFloss(
      Properties properties,
      Permissions permissions);

 protected:
  BluetoothGattCharacteristicFloss();
  ~BluetoothGattCharacteristicFloss() override;
};
}  // namespace floss

#endif  // DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_GATT_CHARACTERISTIC_FLOSS_H_

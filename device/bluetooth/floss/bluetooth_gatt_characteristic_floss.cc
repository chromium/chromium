// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/bluetooth_gatt_characteristic_floss.h"

#include "device/bluetooth/floss/floss_gatt_manager_client.h"

namespace {

using FlossCharacteristic = floss::GattCharacteristic;

constexpr std::pair<FlossCharacteristic::Permission,
                    device::BluetoothGattCharacteristic::Permission>
    kPermissionMapping[] = {
        {FlossCharacteristic::GATT_PERM_READ,
         device::BluetoothGattCharacteristic::PERMISSION_READ},
        {FlossCharacteristic::GATT_PERM_READ_ENCRYPTED,
         device::BluetoothGattCharacteristic::PERMISSION_READ_ENCRYPTED},
        {FlossCharacteristic::GATT_PERM_READ_ENC_MITM,
         device::BluetoothGattCharacteristic::
             PERMISSION_READ_ENCRYPTED_AUTHENTICATED},
        {FlossCharacteristic::GATT_PERM_WRITE,
         device::BluetoothGattCharacteristic::PERMISSION_WRITE},
        {FlossCharacteristic::GATT_PERM_WRITE_ENCRYPTED,
         device::BluetoothGattCharacteristic::PERMISSION_WRITE_ENCRYPTED},
        {FlossCharacteristic::GATT_PERM_WRITE_ENC_MITM,
         device::BluetoothGattCharacteristic::
             PERMISSION_WRITE_ENCRYPTED_AUTHENTICATED},
        {FlossCharacteristic::GATT_PERM_WRITE_SIGNED_MITM,
         device::BluetoothGattCharacteristic::
             PERMISSION_WRITE_ENCRYPTED_AUTHENTICATED},
};
constexpr std::pair<FlossCharacteristic::Property,
                    device::BluetoothGattCharacteristic::Properties>
    kPropertyMapping[] = {
        {FlossCharacteristic::GATT_CHAR_PROP_BIT_BROADCAST,
         device::BluetoothGattCharacteristic::PROPERTY_BROADCAST},
        {FlossCharacteristic::GATT_CHAR_PROP_BIT_READ,
         device::BluetoothGattCharacteristic::PROPERTY_READ},
        {FlossCharacteristic::GATT_CHAR_PROP_BIT_WRITE_NR,
         device::BluetoothGattCharacteristic::PROPERTY_WRITE_WITHOUT_RESPONSE},
        {FlossCharacteristic::GATT_CHAR_PROP_BIT_WRITE,
         device::BluetoothGattCharacteristic::PROPERTY_WRITE},
        {FlossCharacteristic::GATT_CHAR_PROP_BIT_NOTIFY,
         device::BluetoothGattCharacteristic::PROPERTY_NOTIFY},
        {FlossCharacteristic::GATT_CHAR_PROP_BIT_INDICATE,
         device::BluetoothGattCharacteristic::PROPERTY_INDICATE},
        {FlossCharacteristic::GATT_CHAR_PROP_BIT_AUTH,
         device::BluetoothGattCharacteristic::
             PROPERTY_AUTHENTICATED_SIGNED_WRITES},
        {FlossCharacteristic::GATT_CHAR_PROP_BIT_EXT_PROP,
         device::BluetoothGattCharacteristic::PROPERTY_EXTENDED_PROPERTIES},
};

device::BluetoothGattCharacteristic::Permissions ConvertPermissionsFromFloss(
    const uint16_t permissions) {
  device::BluetoothGattCharacteristic::Permissions result =
      device::BluetoothGattCharacteristic::PERMISSION_NONE;

  for (const auto& permission_pair : kPermissionMapping) {
    if (permissions & permission_pair.first) {
      result |= permission_pair.second;
    }
  }
  return result;
}

FlossCharacteristic::Permission ConvertPermissionsToFloss(
    const device::BluetoothGattCharacteristic::Permissions permissions) {
  uint16_t result = 0;
  for (const auto& permission_pair : kPermissionMapping) {
    if (permissions & permission_pair.second) {
      result |= permission_pair.first;
    }
  }

  return static_cast<FlossCharacteristic::Permission>(result);
}

device::BluetoothGattCharacteristic::Properties ConvertPropertiesFromFloss(
    const uint8_t properties) {
  device::BluetoothGattCharacteristic::Properties result =
      device::BluetoothGattCharacteristic::PROPERTY_NONE;

  for (const auto& property_pair : kPropertyMapping) {
    if (properties & property_pair.first) {
      result |= property_pair.second;
    }
  }
  return result;
}

FlossCharacteristic::Property ConvertPropertiesToFloss(
    const device::BluetoothGattCharacteristic::Properties properties) {
  uint8_t result = 0;
  for (const auto& property_pair : kPropertyMapping) {
    if (properties & property_pair.second) {
      result |= property_pair.first;
    }
  }
  return static_cast<FlossCharacteristic::Property>(result);
}

}  // namespace

namespace floss {

BluetoothGattCharacteristicFloss::BluetoothGattCharacteristicFloss() = default;
BluetoothGattCharacteristicFloss::~BluetoothGattCharacteristicFloss() = default;

std::pair<device::BluetoothGattCharacteristic::Properties,
          device::BluetoothGattCharacteristic::Permissions>
BluetoothGattCharacteristicFloss::ConvertPropsAndPermsFromFloss(
    const uint8_t properties,
    const uint16_t permissions) {
  device::BluetoothGattCharacteristic::Permissions out_permissions =
      ConvertPermissionsFromFloss(permissions);
  device::BluetoothGattCharacteristic::Properties out_properties =
      ConvertPropertiesFromFloss(properties);

  // We also need to generate some properties from permissions. The spec only
  // defines 8 bits for properties but device::Bluetooth has more internally.

  if (permissions & FlossCharacteristic::GATT_PERM_READ_ENC_MITM) {
    out_properties |= PROPERTY_READ_ENCRYPTED_AUTHENTICATED;
  } else if (permissions & FlossCharacteristic::GATT_PERM_READ_ENCRYPTED) {
    out_properties |= PROPERTY_READ_ENCRYPTED;
  }

  if (permissions & FlossCharacteristic::GATT_PERM_WRITE_ENC_MITM) {
    out_properties |= PROPERTY_WRITE_ENCRYPTED_AUTHENTICATED;
  } else if (permissions & FlossCharacteristic::GATT_PERM_WRITE_ENCRYPTED) {
    out_properties |= PROPERTY_WRITE_ENCRYPTED;
  }

  // Either signed permission results in signed writes.
  if (permissions & FlossCharacteristic::GATT_PERM_WRITE_SIGNED ||
      permissions & FlossCharacteristic::GATT_PERM_WRITE_SIGNED_MITM) {
    out_properties |= PROPERTY_AUTHENTICATED_SIGNED_WRITES;
  }

  return std::make_pair(out_properties, out_permissions);
}

std::pair<uint8_t, uint16_t>
BluetoothGattCharacteristicFloss::ConvertPropsAndPermsToFloss(
    device::BluetoothGattCharacteristic::Properties properties,
    device::BluetoothGattCharacteristic::Permissions permissions) {
  uint16_t out_permissions =
      static_cast<uint16_t>(ConvertPermissionsToFloss(permissions));
  uint8_t out_properties =
      static_cast<uint8_t>(ConvertPropertiesToFloss(properties));

  return std::make_pair(out_properties, out_permissions);
}

}  // namespace floss

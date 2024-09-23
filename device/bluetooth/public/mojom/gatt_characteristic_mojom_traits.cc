// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/public/mojom/gatt_characteristic_mojom_traits.h"

namespace {
using Properties = device::BluetoothGattCharacteristic::Properties;
using Permissions = device::BluetoothGattCharacteristic::Permissions;
using Property = device::BluetoothGattCharacteristic::Property;
using Permission = device::BluetoothGattCharacteristic::Permission;
}  // namespace

namespace mojo {

bool StructTraits<bluetooth::mojom::GattCharacteristicPropertiesDataView,
                  Properties>::broadcast(const Properties& input) {
  return (input & Property::PROPERTY_BROADCAST) != 0;
}

bool StructTraits<bluetooth::mojom::GattCharacteristicPropertiesDataView,
                  Properties>::read(const Properties& input) {
  return (input & Property::PROPERTY_READ) != 0;
}

bool StructTraits<bluetooth::mojom::GattCharacteristicPropertiesDataView,
                  Properties>::write_without_response(const Properties& input) {
  return (input & Property::PROPERTY_WRITE_WITHOUT_RESPONSE) != 0;
}

bool StructTraits<bluetooth::mojom::GattCharacteristicPropertiesDataView,
                  Properties>::write(const Properties& input) {
  return (input & Property::PROPERTY_WRITE) != 0;
}

bool StructTraits<bluetooth::mojom::GattCharacteristicPropertiesDataView,
                  Properties>::notify(const Properties& input) {
  return (input & Property::PROPERTY_NOTIFY) != 0;
}

bool StructTraits<bluetooth::mojom::GattCharacteristicPropertiesDataView,
                  Properties>::indicate(const Properties& input) {
  return (input & Property::PROPERTY_INDICATE) != 0;
}

bool StructTraits<bluetooth::mojom::GattCharacteristicPropertiesDataView,
                  Properties>::authenticated_signed_writes(const Properties&
                                                               input) {
  return (input & Property::PROPERTY_AUTHENTICATED_SIGNED_WRITES) != 0;
}

bool StructTraits<bluetooth::mojom::GattCharacteristicPropertiesDataView,
                  Properties>::extended_properties(const Properties& input) {
  return (input & Property::PROPERTY_EXTENDED_PROPERTIES) != 0;
}

bool StructTraits<bluetooth::mojom::GattCharacteristicPropertiesDataView,
                  Properties>::reliable_write(const Properties& input) {
  return (input & Property::PROPERTY_RELIABLE_WRITE) != 0;
}

bool StructTraits<bluetooth::mojom::GattCharacteristicPropertiesDataView,
                  Properties>::writable_auxiliaries(const Properties& input) {
  return (input & Property::PROPERTY_WRITABLE_AUXILIARIES) != 0;
}

bool StructTraits<bluetooth::mojom::GattCharacteristicPropertiesDataView,
                  Properties>::read_encrypted(const Properties& input) {
  return (input & Property::PROPERTY_READ_ENCRYPTED) != 0;
}

bool StructTraits<bluetooth::mojom::GattCharacteristicPropertiesDataView,
                  Properties>::write_encrypted(const Properties& input) {
  return (input & Property::PROPERTY_WRITE_ENCRYPTED) != 0;
}

bool StructTraits<bluetooth::mojom::GattCharacteristicPropertiesDataView,
                  Properties>::read_encrypted_authenticated(const Properties&
                                                                input) {
  return (input & Property::PROPERTY_READ_ENCRYPTED_AUTHENTICATED) != 0;
}

bool StructTraits<bluetooth::mojom::GattCharacteristicPropertiesDataView,
                  Properties>::write_encrypted_authenticated(const Properties&
                                                                 input) {
  return (input & Property::PROPERTY_WRITE_ENCRYPTED_AUTHENTICATED) != 0;
}

bool StructTraits<bluetooth::mojom::GattCharacteristicPropertiesDataView,
                  Properties>::
    Read(bluetooth::mojom::GattCharacteristicPropertiesDataView input,
         Properties* output) {
  *output = Property::PROPERTY_NONE;

  if (input.broadcast()) {
    *output |= Property::PROPERTY_BROADCAST;
  }

  if (input.read()) {
    *output |= Property::PROPERTY_READ;
  }

  if (input.write_without_response()) {
    *output |= Property::PROPERTY_WRITE_WITHOUT_RESPONSE;
  }

  if (input.write()) {
    *output |= Property::PROPERTY_WRITE;
  }

  if (input.notify()) {
    *output |= Property::PROPERTY_NOTIFY;
  }

  if (input.indicate()) {
    *output |= Property::PROPERTY_INDICATE;
  }

  if (input.authenticated_signed_writes()) {
    *output |= Property::PROPERTY_AUTHENTICATED_SIGNED_WRITES;
  }

  if (input.extended_properties()) {
    *output |= Property::PROPERTY_EXTENDED_PROPERTIES;
  }

  if (input.reliable_write()) {
    *output |= Property::PROPERTY_RELIABLE_WRITE;
  }

  if (input.writable_auxiliaries()) {
    *output |= Property::PROPERTY_WRITABLE_AUXILIARIES;
  }

  if (input.read_encrypted()) {
    *output |= Property::PROPERTY_READ_ENCRYPTED;
  }

  if (input.write_encrypted()) {
    *output |= Property::PROPERTY_WRITE_ENCRYPTED;
  }

  if (input.read_encrypted_authenticated()) {
    *output |= Property::PROPERTY_READ_ENCRYPTED_AUTHENTICATED;
  }

  if (input.write_encrypted_authenticated()) {
    *output |= Property::PROPERTY_WRITE_ENCRYPTED_AUTHENTICATED;
  }

  return true;
}

bool StructTraits<bluetooth::mojom::GattCharacteristicPermissionsDataView,
                  Permissions>::read(const Permissions& input) {
  return (input & Permission::PERMISSION_READ) != 0;
}

bool StructTraits<bluetooth::mojom::GattCharacteristicPermissionsDataView,
                  Permissions>::write(const Permissions& input) {
  return (input & Permission::PERMISSION_WRITE) != 0;
}

bool StructTraits<bluetooth::mojom::GattCharacteristicPermissionsDataView,
                  Permissions>::read_encrypted(const Permissions& input) {
  return (input & Permission::PERMISSION_READ_ENCRYPTED) != 0;
}

bool StructTraits<bluetooth::mojom::GattCharacteristicPermissionsDataView,
                  Permissions>::write_encrypted(const Permissions& input) {
  return (input & Permission::PERMISSION_WRITE_ENCRYPTED) != 0;
}

bool StructTraits<bluetooth::mojom::GattCharacteristicPermissionsDataView,
                  Permissions>::read_encrypted_authenticated(const Permissions&
                                                                 input) {
  return (input & Permission::PERMISSION_READ_ENCRYPTED_AUTHENTICATED) != 0;
}

bool StructTraits<bluetooth::mojom::GattCharacteristicPermissionsDataView,
                  Permissions>::write_encrypted_authenticated(const Permissions&
                                                                  input) {
  return (input & Permission::PERMISSION_WRITE_ENCRYPTED_AUTHENTICATED) != 0;
}

bool StructTraits<bluetooth::mojom::GattCharacteristicPermissionsDataView,
                  Permissions>::
    Read(bluetooth::mojom::GattCharacteristicPermissionsDataView input,
         Permissions* output) {
  *output = Permission::PERMISSION_NONE;

  if (input.read()) {
    *output |= Permission::PERMISSION_READ;
  }

  if (input.write()) {
    *output |= Permission::PERMISSION_WRITE;
  }

  if (input.read_encrypted()) {
    *output |= Permission::PERMISSION_READ_ENCRYPTED;
  }

  if (input.write_encrypted()) {
    *output |= Permission::PERMISSION_WRITE_ENCRYPTED;
  }

  if (input.read_encrypted_authenticated()) {
    *output |= Permission::PERMISSION_READ_ENCRYPTED_AUTHENTICATED;
  }

  if (input.write_encrypted_authenticated()) {
    *output |= Permission::PERMISSION_WRITE_ENCRYPTED_AUTHENTICATED;
  }

  return true;
}

}  // namespace mojo

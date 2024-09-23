// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_GATT_CHARACTERISTIC_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_GATT_CHARACTERISTIC_H_

#include <stdint.h>
#include <string>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/bluetooth_gatt_service.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

namespace device {

// BluetoothGattCharacteristic represents a local or remote GATT characteristic.
// A GATT characteristic is a basic data element used to construct a GATT
// service. Hence, instances of a BluetoothGattCharacteristic are associated
// with a BluetoothGattService.
class DEVICE_BLUETOOTH_EXPORT BluetoothGattCharacteristic {
 public:
  // Values representing the possible properties of a characteristic, which
  // define how the characteristic can be used. Each of these properties serve
  // a role as defined in the Bluetooth Specification.
  // |PROPERTY_EXTENDED_PROPERTIES| is a special property that, if present,
  // indicates that there is a characteristic descriptor (namely the
  // "Characteristic Extended Properties Descriptor" with UUID 0x2900) that
  // contains additional properties pertaining to the characteristic.
  // The properties "ReliableWrite| and |WriteAuxiliaries| are retrieved from
  // that characteristic.
  enum Property {
    PROPERTY_NONE = 0,
    PROPERTY_BROADCAST = 1 << 0,
    PROPERTY_READ = 1 << 1,
    PROPERTY_WRITE_WITHOUT_RESPONSE = 1 << 2,
    PROPERTY_WRITE = 1 << 3,
    PROPERTY_NOTIFY = 1 << 4,
    PROPERTY_INDICATE = 1 << 5,
    PROPERTY_AUTHENTICATED_SIGNED_WRITES = 1 << 6,
    PROPERTY_EXTENDED_PROPERTIES = 1 << 7,
    PROPERTY_RELIABLE_WRITE = 1 << 8,
    PROPERTY_WRITABLE_AUXILIARIES = 1 << 9,
    PROPERTY_READ_ENCRYPTED = 1 << 10,
    PROPERTY_WRITE_ENCRYPTED = 1 << 11,
    PROPERTY_READ_ENCRYPTED_AUTHENTICATED = 1 << 12,
    PROPERTY_WRITE_ENCRYPTED_AUTHENTICATED = 1 << 13,
    NUM_PROPERTY = 1 << 14,
  };
  typedef uint32_t Properties;

  // Values representing read, write, and encryption permissions for a
  // characteristic's value. While attribute permissions for all GATT
  // definitions have been set by the Bluetooth specification, characteristic
  // value permissions are left up to the higher-level profile.
  //
  // Attribute permissions are distinct from the characteristic properties. For
  // example, a characteristic may have the property |PROPERTY_READ| to make
  // clients know that it is possible to read the characteristic value and have
  // the permission |PERMISSION_READ_ENCRYPTED| to require a secure connection.
  // It is up to the application to properly specify the permissions and
  // properties for a local characteristic.
  // TODO(rkc): Currently BlueZ infers permissions for characteristics from
  // the properties. Once this is fixed, we will start sending the permissions
  // for characteristics to BlueZ. Till then permissions for characteristics
  // are unimplemented.
  enum Permission {
    PERMISSION_NONE = 0,
    PERMISSION_READ = 1 << 0,
    PERMISSION_WRITE = 1 << 1,
    PERMISSION_READ_ENCRYPTED = 1 << 2,
    PERMISSION_WRITE_ENCRYPTED = 1 << 3,
    PERMISSION_READ_ENCRYPTED_AUTHENTICATED = 1 << 4,
    PERMISSION_WRITE_ENCRYPTED_AUTHENTICATED = 1 << 5,
    NUM_PERMISSION = 1 << 6,
  };
  typedef uint32_t Permissions;

  // Bluetooth Spec Vol 3, Part G, 3.3.3.3 Client Characteristic Configuration.
  enum class NotificationType : uint16_t { kNone, kNotification, kIndication };

  // The ErrorCallback is used by methods to asynchronously report errors.
  using ErrorCallback =
      base::OnceCallback<void(BluetoothGattService::GattErrorCode)>;

  BluetoothGattCharacteristic(const BluetoothGattCharacteristic&) = delete;
  BluetoothGattCharacteristic& operator=(const BluetoothGattCharacteristic&) =
      delete;

  // Identifier used to uniquely identify a GATT characteristic object. This is
  // different from the characteristic UUID: while multiple characteristics with
  // the same UUID can exist on a Bluetooth device, the identifier returned from
  // this method is unique among all characteristics on the adapter. The
  // contents of the identifier are platform specific.
  virtual std::string GetIdentifier() const = 0;

  // The Bluetooth-specific UUID of the characteristic.
  virtual BluetoothUUID GetUUID() const = 0;

  // Returns the bitmask of characteristic properties.
  virtual Properties GetProperties() const = 0;

  // Returns the bitmask of characteristic attribute permissions.
  virtual Permissions GetPermissions() const = 0;

 protected:
  BluetoothGattCharacteristic();
  virtual ~BluetoothGattCharacteristic();
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_GATT_CHARACTERISTIC_H_

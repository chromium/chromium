// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_GATT_DESCRIPTOR_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_GATT_DESCRIPTOR_H_

#include <string>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_gatt_service.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

namespace device {

// BluetoothGattDescriptor represents a local or remote GATT characteristic
// descriptor. A GATT characteristic descriptor provides further information
// about a characteristic's value. They can be used to describe the
// characteristic's features or to control certain behaviors.
class DEVICE_BLUETOOTH_EXPORT BluetoothGattDescriptor {
 public:
  // The ErrorCallback is used by methods to asynchronously report errors.
  using ErrorCallback =
      base::OnceCallback<void(BluetoothGattService::GattErrorCode)>;

  BluetoothGattDescriptor(const BluetoothGattDescriptor&) = delete;
  BluetoothGattDescriptor& operator=(const BluetoothGattDescriptor&) = delete;

  // The Bluetooth Specification declares several predefined descriptors that
  // profiles can use. The following are definitions for the list of UUIDs
  // and descriptions of the characteristic descriptors that they represent.
  // Possible values for and further information on each descriptor can be found
  // in Core v4.0, Volume 3, Part G, Section 3.3.3. All of these descriptors are
  // optional and may not be present for a given characteristic.

  // The "Characteristic Extended Properties" descriptor. This defines
  // additional "Characteristic Properties" which cannot fit into the allocated
  // single octet property field of a characteristic. The value is a bit field
  // and the two predefined bits, as per Bluetooth Core Specification v4.0, are:
  //
  //    - Reliable Write: 0x0001
  //    - Writable Auxiliaries: 0x0002
  //
  static const BluetoothUUID& CharacteristicExtendedPropertiesUuid();

  // The "Characteristic User Description" descriptor defines a UTF-8 string of
  // variable size that is a user textual description of the associated
  // characteristic's value. There can be only one instance of this descriptor
  // per characteristic. This descriptor can be written to if the "Writable
  // Auxiliaries" bit of the Characteristic Properties (via the "Characteristic
  // Extended Properties" descriptor) has been set.
  static const BluetoothUUID& CharacteristicUserDescriptionUuid();

  // The "Client Characteristic Configuration" descriptor defines how the
  // characteristic may be configured by a specific client. A server-side
  // instance of this descriptor exists for each client that has bonded with
  // the server and the value can be read and written by that client only. As
  // of Core v4.0, this descriptor is used by clients to set up notifications
  // and indications from a characteristic. The value is a bit field and the
  // predefined bits are:
  //
  //    - Default: 0x0000
  //    - Notification: 0x0001
  //    - Indication: 0x0002
  //
  static const BluetoothUUID& ClientCharacteristicConfigurationUuid();

  // The "Server Characteristic Configuration" descriptor defines how the
  // characteristic may be configured for the server. There is one instance
  // of this descriptor for all clients and setting the value of this descriptor
  // affects its configuration for all clients. As of Core v4.0, this descriptor
  // is used to set up the server to broadcast the characteristic value if
  // advertising resources are available. The value is a bit field and the
  // predefined bits are:
  //
  //    - Default: 0x0000
  //    - Broadcast: 0x0001
  //
  static const BluetoothUUID& ServerCharacteristicConfigurationUuid();

  // The "Characteristic Presentation Format" declaration defines the format of
  // the Characteristic Value. The value is composed of 7 octets which are
  // divided into groups that represent different semantic meanings. For a
  // detailed description of how the value of this descriptor should be
  // interpreted, refer to Core v4.0, Volume 3, Part G, Section 3.3.3.5. If more
  // than one declaration of this descriptor exists for a characteristic, then a
  // "Characteristic Aggregate Format" descriptor must also exist for that
  // characteristic.
  static const BluetoothUUID& CharacteristicPresentationFormatUuid();

  // The "Characteristic Aggregate Format" descriptor defines the format of an
  // aggregated characteristic value. In GATT's underlying protocol, ATT, each
  // attribute is identified by a handle that is unique for the hosting server.
  // Multiple characteristics can share the same instance(s) of a
  // "Characteristic Presentation Format" descriptor. The value of the
  // "Characteristic Aggregate Format" descriptor contains a list of handles
  // that each refer to a "Characteristic Presentation Format" descriptor that
  // is used by that characteristic. Hence, exactly one instance of this
  // descriptor must exist if more than one "Characteristic Presentation Format"
  // descriptors exist for a characteristic.
  //
  // Applications that are using the device::Bluetooth API do not have access to
  // the underlying handles and shouldn't use this descriptor to determine which
  // "Characteristic Presentation Format" descriptors belong to a
  // characteristic.
  // The API will construct a BluetoothGattDescriptor object for each instance
  // of "Characteristic Presentation Format" descriptor per instance of
  // BluetoothRemoteGattCharacteristic that represents a remote characteristic.
  // Similarly for local characteristics, implementations DO NOT need to create
  // an instance of BluetoothGattDescriptor for this descriptor as this will be
  // handled by the subsystem.
  static const BluetoothUUID& CharacteristicAggregateFormatUuid();

  // Identifier used to uniquely identify a GATT descriptor object. This is
  // different from the descriptor UUID: while multiple descriptors with the
  // same UUID can exist on a Bluetooth device, the identifier returned from
  // this method is unique among all descriptors on the adapter. The contents of
  // the identifier are platform specific.
  virtual std::string GetIdentifier() const = 0;

  // The Bluetooth-specific UUID of the characteristic descriptor.
  virtual BluetoothUUID GetUUID() const = 0;

  // Returns the bitmask of characteristic descriptor attribute permissions.
  virtual BluetoothGattCharacteristic::Permissions GetPermissions() const = 0;

 protected:
  BluetoothGattDescriptor();
  virtual ~BluetoothGattDescriptor();
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_GATT_DESCRIPTOR_H_

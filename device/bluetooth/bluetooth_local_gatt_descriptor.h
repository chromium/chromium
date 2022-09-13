// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_LOCAL_GATT_DESCRIPTOR_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_LOCAL_GATT_DESCRIPTOR_H_

#include <stdint.h>

#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/bluetooth_gatt_descriptor.h"
#include "device/bluetooth/bluetooth_local_gatt_characteristic.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

namespace device {

// BluetoothLocalGattDescriptor represents a local GATT characteristic
// descriptor. A GATT characteristic descriptor provides further information
// about a characteristic's value. They can be used to describe the
// characteristic's features or to control certain behaviors.
//
// Note: We use virtual inheritance on the GATT descriptor since it will be
// inherited by platform specific versions of the GATT descriptor classes also.
// The platform specific local GATT descriptor classes will inherit both this
// class and their GATT descriptor class, hence causing an inheritance diamond.
class DEVICE_BLUETOOTH_EXPORT BluetoothLocalGattDescriptor
    : public virtual BluetoothGattDescriptor {
 public:
  // Constructs a BluetoothLocalGattDescriptor that can be associated with a
  // local GATT characteristic when the adapter is in the peripheral role. To
  // associate the returned descriptor with a characteristic, provide a pointer
  // to that characteristic instance to the create function.
  //
  // This method constructs a characteristic descriptor with UUID |uuid| and the
  // initial cached value |value|. |value| will be cached and returned for read
  // requests and automatically modified for write requests by default, unless
  // an instance of BluetoothLocalGattService::Delegate has been provided to
  // the
  // associated BluetoothLocalGattService instance, in which case the delegate
  // will
  // handle the read and write requests.
  //
  // Currently, only custom UUIDs, |kCharacteristicDescriptionUuid|, and
  // |kCharacteristicPresentationFormat| are supported for locally hosted
  // descriptors. This method will return NULL if |uuid| is any one of the
  // unsupported predefined descriptor UUIDs.
  static base::WeakPtr<BluetoothLocalGattDescriptor> Create(
      const BluetoothUUID& uuid,
      BluetoothGattCharacteristic::Permissions permissions,
      BluetoothLocalGattCharacteristic* characteristic);

  BluetoothLocalGattDescriptor(const BluetoothLocalGattDescriptor&) = delete;
  BluetoothLocalGattDescriptor& operator=(const BluetoothLocalGattDescriptor&) =
      delete;

  virtual BluetoothLocalGattCharacteristic* GetCharacteristic() const = 0;

 protected:
  BluetoothLocalGattDescriptor();
  ~BluetoothLocalGattDescriptor() override;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_LOCAL_GATT_DESCRIPTOR_H_

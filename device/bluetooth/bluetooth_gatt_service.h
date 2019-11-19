// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_GATT_SERVICE_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_GATT_SERVICE_H_

#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

namespace device {

// BluetoothGattService represents a local or remote GATT service. A GATT
// service is hosted by a peripheral and represents a collection of data in
// the form of GATT characteristics and a set of included GATT services if this
// service is what is called "a primary service".
class DEVICE_BLUETOOTH_EXPORT BluetoothGattService {
 public:
  // Interacting with Characteristics and Descriptors can produce
  // this set of errors.
  enum GattErrorCode {
    GATT_ERROR_UNKNOWN = 0,
    GATT_ERROR_FAILED,
    GATT_ERROR_IN_PROGRESS,
    GATT_ERROR_INVALID_LENGTH,
    GATT_ERROR_NOT_PERMITTED,
    GATT_ERROR_NOT_AUTHORIZED,
    GATT_ERROR_NOT_PAIRED,
    GATT_ERROR_NOT_SUPPORTED
  };

  // The ErrorCallback is used by methods to asynchronously report errors.
  using ErrorCallback = base::Callback<void(GattErrorCode error_code)>;

  // Identifier used to uniquely identify a GATT service object. This is
  // different from the service UUID: while multiple services with the same UUID
  // can exist on a Bluetooth device, the identifier returned from this method
  // is unique among all services on the adapter. The contents of the identifier
  // are platform specific.
  virtual std::string GetIdentifier() const = 0;

  // The Bluetooth-specific UUID of the service.
  virtual BluetoothUUID GetUUID() const = 0;

  // Indicates whether the type of this service is primary or secondary. A
  // primary service describes the primary function of the peripheral that
  // hosts it, while a secondary service only makes sense in the presence of a
  // primary service. A primary service may include other primary or secondary
  // services.
  virtual bool IsPrimary() const = 0;

  virtual ~BluetoothGattService();

 protected:
  BluetoothGattService();

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothGattService);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_GATT_SERVICE_H_

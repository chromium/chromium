// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_GATT_SERVICE_FLOSS_H_
#define DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_GATT_SERVICE_FLOSS_H_

#include "device/bluetooth/bluetooth_gatt_service.h"
#include "device/bluetooth/floss/floss_gatt_client.h"

namespace floss {

class BluetoothAdapterFloss;

// Subclass of |BluetoothGattService| for platforms that use Floss.
class DEVICE_BLUETOOTH_EXPORT BluetoothGattServiceFloss
    : public device::BluetoothGattService {
 public:
  BluetoothGattServiceFloss(const BluetoothGattServiceFloss&) = delete;
  BluetoothGattServiceFloss& operator=(const BluetoothGattServiceFloss&) =
      delete;

  // Returns the adapter associated with this service.
  BluetoothAdapterFloss* GetAdapter() const;

  // Processes a |GattStatus| into a service error code.
  static device::BluetoothGattService::GattErrorCode GattStatusToServiceError(
      const GattStatus status);

 protected:
  explicit BluetoothGattServiceFloss(BluetoothAdapterFloss* adapter);
  ~BluetoothGattServiceFloss() override;

 private:
  // The adapter associated with (and which indirectly owns) this service.
  raw_ptr<BluetoothAdapterFloss> adapter_;
};

}  // namespace floss

#endif  // DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_GATT_SERVICE_FLOSS_H_

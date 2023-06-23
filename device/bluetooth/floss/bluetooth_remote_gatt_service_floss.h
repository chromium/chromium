// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_REMOTE_GATT_SERVICE_FLOSS_H_
#define DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_REMOTE_GATT_SERVICE_FLOSS_H_

#include <memory>

#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/bluetooth_remote_gatt_service.h"
#include "device/bluetooth/floss/bluetooth_gatt_service_floss.h"
#include "device/bluetooth/floss/floss_gatt_manager_client.h"

namespace device {
class BluetoothDevice;
}

namespace floss {

class BluetoothAdapterFloss;
class BluetoothDeviceFloss;

class DEVICE_BLUETOOTH_EXPORT BluetoothRemoteGattServiceFloss
    : public BluetoothGattServiceFloss,
      public device::BluetoothRemoteGattService {
 public:
  static std::unique_ptr<BluetoothRemoteGattServiceFloss> Create(
      BluetoothAdapterFloss* adapter,
      BluetoothDeviceFloss* device,
      GattService remote_service);

  BluetoothRemoteGattServiceFloss(const BluetoothRemoteGattServiceFloss&) =
      delete;
  BluetoothRemoteGattServiceFloss& operator=(
      const BluetoothRemoteGattServiceFloss&) = delete;

  ~BluetoothRemoteGattServiceFloss() override;

  // device::BluetoothRemoteGattService overrides.
  std::string GetIdentifier() const override;
  device::BluetoothUUID GetUUID() const override;
  device::BluetoothDevice* GetDevice() const override;
  bool IsPrimary() const override;
  std::vector<device::BluetoothRemoteGattService*> GetIncludedServices()
      const override;

 protected:
  BluetoothRemoteGattServiceFloss(BluetoothAdapterFloss* adapter,
                                  BluetoothDeviceFloss* device,
                                  GattService remote_service);

 private:
  // Data about the remote gatt service represented by this class.
  GattService remote_service_;

  // Services that are included with this service.
  std::vector<std::unique_ptr<BluetoothRemoteGattServiceFloss>>
      included_services_;

  // The device this GATT service belongs to. Ok to store raw pointer here since
  // |device_| owns this instance.
  raw_ptr<BluetoothDeviceFloss> device_;
};

}  // namespace floss

#endif  // DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_REMOTE_GATT_SERVICE_FLOSS_H_

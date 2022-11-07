// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_CAST_BLUETOOTH_REMOTE_GATT_SERVICE_CAST_H_
#define DEVICE_BLUETOOTH_CAST_BLUETOOTH_REMOTE_GATT_SERVICE_CAST_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "device/bluetooth/bluetooth_remote_gatt_service.h"

namespace chromecast {
namespace bluetooth {
class RemoteService;
}  // namespace bluetooth
}  // namespace chromecast

namespace device {

class BluetoothDeviceCast;

class BluetoothRemoteGattServiceCast : public BluetoothRemoteGattService {
 public:
  BluetoothRemoteGattServiceCast(
      BluetoothDeviceCast* device,
      scoped_refptr<chromecast::bluetooth::RemoteService> remote_service);

  BluetoothRemoteGattServiceCast(const BluetoothRemoteGattServiceCast&) =
      delete;
  BluetoothRemoteGattServiceCast& operator=(
      const BluetoothRemoteGattServiceCast&) = delete;

  ~BluetoothRemoteGattServiceCast() override;

  // BluetoothGattService implementation:
  std::string GetIdentifier() const override;
  BluetoothUUID GetUUID() const override;
  bool IsPrimary() const override;

  // BluetoothRemoteGattService implementation:
  BluetoothDevice* GetDevice() const override;
  std::vector<BluetoothRemoteGattService*> GetIncludedServices() const override;

 private:
  BluetoothDeviceCast* const device_;
  scoped_refptr<chromecast::bluetooth::RemoteService> remote_service_;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_CAST_BLUETOOTH_REMOTE_GATT_SERVICE_CAST_H_

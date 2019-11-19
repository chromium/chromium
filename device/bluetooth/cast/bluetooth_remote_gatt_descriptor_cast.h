// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_CAST_BLUETOOTH_REMOTE_GATT_DESCRIPTOR_CAST_H_
#define DEVICE_BLUETOOTH_CAST_BLUETOOTH_REMOTE_GATT_DESCRIPTOR_CAST_H_

#include <stdint.h>

#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_remote_gatt_descriptor.h"

namespace chromecast {
namespace bluetooth {
class RemoteDescriptor;
}  // namespace bluetooth
}  // namespace chromecast

namespace device {

class BluetoothRemoteGattCharacteristicCast;

class BluetoothRemoteGattDescriptorCast : public BluetoothRemoteGattDescriptor {
 public:
  BluetoothRemoteGattDescriptorCast(
      BluetoothRemoteGattCharacteristicCast* characteristic,
      scoped_refptr<chromecast::bluetooth::RemoteDescriptor> remote_descriptor);
  ~BluetoothRemoteGattDescriptorCast() override;

  // BluetoothGattDescriptor implementation:
  std::string GetIdentifier() const override;
  BluetoothUUID GetUUID() const override;
  BluetoothGattCharacteristic::Permissions GetPermissions() const override;

  // BluetoothRemoteGattDescriptor implementation:
  const std::vector<uint8_t>& GetValue() const override;
  BluetoothRemoteGattCharacteristic* GetCharacteristic() const override;
  void ReadRemoteDescriptor(ValueCallback callback,
                            ErrorCallback error_callback) override;
  void WriteRemoteDescriptor(const std::vector<uint8_t>& new_value,
                             base::OnceClosure callback,
                             ErrorCallback error_callback) override;

 private:
  // Called when the remote descriptor has been read or the operation has
  // failed. If the former, |success| will be true, and |result| will be
  // valid. In this case, |value_| is updated and |callback| is run with
  // |result|. If |success| is false, |result| is ignored and |error_callback|
  // is run.
  void OnReadRemoteDescriptor(ValueCallback callback,
                              ErrorCallback error_callback,
                              bool success,
                              const std::vector<uint8_t>& result);

  // Called back when the remote descriptor has been written or the operation
  // has failed. Each of the parameters corresponds to a parameter to
  // WriteRemoteDescriptor(), and |success| is true if the write was successful.
  // If successful, |value_| will be updated.
  void OnWriteRemoteDescriptor(const std::vector<uint8_t>& written_value,
                               base::OnceClosure callback,
                               ErrorCallback error_callback,
                               bool success);

  BluetoothRemoteGattCharacteristicCast* const characteristic_;
  scoped_refptr<chromecast::bluetooth::RemoteDescriptor> remote_descriptor_;
  std::vector<uint8_t> value_;

  base::WeakPtrFactory<BluetoothRemoteGattDescriptorCast> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(BluetoothRemoteGattDescriptorCast);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_CAST_BLUETOOTH_REMOTE_GATT_DESCRIPTOR_CAST_H_

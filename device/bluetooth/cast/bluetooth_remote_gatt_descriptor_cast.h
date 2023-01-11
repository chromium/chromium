// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_CAST_BLUETOOTH_REMOTE_GATT_DESCRIPTOR_CAST_H_
#define DEVICE_BLUETOOTH_CAST_BLUETOOTH_REMOTE_GATT_DESCRIPTOR_CAST_H_

#include <stdint.h>

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
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

  BluetoothRemoteGattDescriptorCast(const BluetoothRemoteGattDescriptorCast&) =
      delete;
  BluetoothRemoteGattDescriptorCast& operator=(
      const BluetoothRemoteGattDescriptorCast&) = delete;

  ~BluetoothRemoteGattDescriptorCast() override;

  // BluetoothGattDescriptor implementation:
  std::string GetIdentifier() const override;
  BluetoothUUID GetUUID() const override;
  BluetoothGattCharacteristic::Permissions GetPermissions() const override;

  // BluetoothRemoteGattDescriptor implementation:
  const std::vector<uint8_t>& GetValue() const override;
  BluetoothRemoteGattCharacteristic* GetCharacteristic() const override;
  void ReadRemoteDescriptor(ValueCallback callback) override;
  void WriteRemoteDescriptor(const std::vector<uint8_t>& new_value,
                             base::OnceClosure callback,
                             ErrorCallback error_callback) override;

 private:
  // Called when the remote descriptor has been read or the operation has
  // failed. If the former, |success| will be true, and |result| will be
  // valid. In this case, |value_| is updated and |callback| is run with
  // |result|. If |success| is false, |callback| will be called with
  // an appropriate error_code and the value should be ignored.
  void OnReadRemoteDescriptor(ValueCallback callback,
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
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_CAST_BLUETOOTH_REMOTE_GATT_DESCRIPTOR_CAST_H_

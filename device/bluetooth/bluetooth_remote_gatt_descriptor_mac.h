// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_DESCRIPTOR_MAC_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_DESCRIPTOR_MAC_H_

#include "device/bluetooth/bluetooth_remote_gatt_descriptor.h"

#import <CoreBluetooth/CoreBluetooth.h>

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

namespace device {

class BluetoothRemoteGattCharacteristicMac;

// The BluetoothRemoteGattDescriptorMac class implements
// BluetoothRemoteGattDescriptor for remote GATT services on macOS.
class DEVICE_BLUETOOTH_EXPORT BluetoothRemoteGattDescriptorMac
    : public BluetoothRemoteGattDescriptor {
 public:
  BluetoothRemoteGattDescriptorMac(
      BluetoothRemoteGattCharacteristicMac* characteristic,
      CBDescriptor* descriptor);
  ~BluetoothRemoteGattDescriptorMac() override;

  // BluetoothGattDescriptor
  std::string GetIdentifier() const override;
  BluetoothUUID GetUUID() const override;
  BluetoothGattCharacteristic::Permissions GetPermissions() const override;
  // BluetoothRemoteGattDescriptor
  const std::vector<uint8_t>& GetValue() const override;
  BluetoothRemoteGattCharacteristic* GetCharacteristic() const override;
  void ReadRemoteDescriptor(ValueCallback callback) override;
  void WriteRemoteDescriptor(const std::vector<uint8_t>& new_value,
                             base::OnceClosure callback,
                             ErrorCallback error_callback) override;

 private:
  friend class BluetoothLowEnergyDeviceMac;
  friend class BluetoothRemoteGattCharacteristicMac;
  friend class BluetoothTestMac;

  // Calls callbacks, when -[id<CBPeripheralDelegate>
  // peripheral:didUpdateValueForDescriptor:error:] is called.
  void DidUpdateValueForDescriptor(NSError* error);
  // Calls callbacks, when -[id<CBPeripheralDelegate>
  // peripheral:didWriteValueForDescriptor:error:] is called.
  void DidWriteValueForDescriptor(NSError* error);
  bool HasPendingRead() const { return !read_value_callback_.is_null(); }
  bool HasPendingWrite() const {
    return !write_value_callbacks_.first.is_null();
  }

  // Returns CoreBluetooth peripheral.
  CBPeripheral* GetCBPeripheral() const;
  // Returns CoreBluetooth descriptor.
  CBDescriptor* GetCBDescriptor() const;
  // gatt_characteristic_ owns instances of this class.
  raw_ptr<BluetoothRemoteGattCharacteristicMac> gatt_characteristic_;
  // Descriptor from CoreBluetooth.
  CBDescriptor* __strong cb_descriptor_;
  // Descriptor identifier.
  std::string identifier_;
  // Descriptor UUID.
  BluetoothUUID uuid_;
  // Descriptor value.
  std::vector<uint8_t> value_;
  // The destructor runs callbacks. Methods can use |destructor_called_| to
  // protect against reentrant calls to a partially deleted instance.
  bool destructor_called_ = false;
  // ReadRemoteDescriptor request callback.
  ValueCallback read_value_callback_;
  // WriteRemoteDescriptor request callbacks.
  std::pair<base::OnceClosure, ErrorCallback> write_value_callbacks_;
};

// Stream operator for logging.
DEVICE_BLUETOOTH_EXPORT std::ostream& operator<<(
    std::ostream& out,
    const BluetoothRemoteGattDescriptorMac& descriptor);

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_DESCRIPTOR_MAC_H_

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_DESCRIPTOR_MAC_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_DESCRIPTOR_MAC_H_

#include "device/bluetooth/bluetooth_remote_gatt_descriptor.h"

#include <vector>

#include "base/mac/scoped_nsobject.h"
#include "base/memory/weak_ptr.h"

#if defined(__OBJC__)
#import <CoreBluetooth/CoreBluetooth.h>
#endif  // defined(__OBJC__)

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
  void ReadRemoteDescriptor(ValueCallback callback,
                            ErrorCallback error_callback) override;
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

  // Returns CoreBluetooth peripheral.
  CBPeripheral* GetCBPeripheral() const;
  // Returns CoreBluetooth descriptor.
  CBDescriptor* GetCBDescriptor() const;
  // gatt_characteristic_ owns instances of this class.
  BluetoothRemoteGattCharacteristicMac* gatt_characteristic_;
  // Descriptor from CoreBluetooth.
  base::scoped_nsobject<CBDescriptor> cb_descriptor_;
  // Descriptor identifier.
  std::string identifier_;
  // Descriptor UUID.
  BluetoothUUID uuid_;
  // Descriptor value.
  std::vector<uint8_t> value_;
  // True if a gatt read or write request is in progress.
  bool value_read_or_write_in_progress_;
  // ReadRemoteDescriptor request callbacks.
  std::pair<ValueCallback, ErrorCallback> read_value_callbacks_;
  // WriteRemoteDescriptor request callbacks.
  std::pair<base::OnceClosure, ErrorCallback> write_value_callbacks_;
};

// Stream operator for logging.
DEVICE_BLUETOOTH_EXPORT std::ostream& operator<<(
    std::ostream& out,
    const BluetoothRemoteGattDescriptorMac& descriptor);

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_CHARACTERISTIC_MAC_H_

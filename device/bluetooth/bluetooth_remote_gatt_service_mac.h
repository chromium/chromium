// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_SERVICE_MAC_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_SERVICE_MAC_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "device/bluetooth/bluetooth_remote_gatt_service.h"

@class CBCharacteristic;
@class CBDescriptor;
@class CBPeripheral;
@class CBService;

namespace device {

class BluetoothDevice;
class BluetoothLowEnergyAdapterApple;
class BluetoothLowEnergyDeviceMac;
class BluetoothRemoteGattCharacteristicMac;
class BluetoothRemoteGattDescriptorMac;

class DEVICE_BLUETOOTH_EXPORT BluetoothRemoteGattServiceMac
    : public BluetoothRemoteGattService {
 public:
  BluetoothRemoteGattServiceMac(
      BluetoothLowEnergyDeviceMac* bluetooth_device_mac,
      CBService* service,
      bool is_primary);

  BluetoothRemoteGattServiceMac(const BluetoothRemoteGattServiceMac&) = delete;
  BluetoothRemoteGattServiceMac& operator=(
      const BluetoothRemoteGattServiceMac&) = delete;

  ~BluetoothRemoteGattServiceMac() override;

  // BluetoothRemoteGattService override.
  std::string GetIdentifier() const override;
  BluetoothUUID GetUUID() const override;
  bool IsPrimary() const override;
  BluetoothDevice* GetDevice() const override;
  std::vector<BluetoothRemoteGattService*> GetIncludedServices() const override;

 private:
  friend class BluetoothLowEnergyDeviceMac;
  friend class BluetoothRemoteGattCharacteristicMac;
  friend class BluetoothTestMac;

  // Starts discovering characteristics by calling CoreBluetooth.
  void DiscoverCharacteristics();
  // Called by the BluetoothLowEnergyDeviceMac instance when the characteristics
  // has been discovered.
  void DidDiscoverCharacteristics();
  // Called by the BluetoothLowEnergyDeviceMac instance when the descriptors has
  // been discovered.
  void DidDiscoverDescriptors(CBCharacteristic* characteristic);
  // Sends notification if this service is ready with all characteristics
  // discovered.
  void SendNotificationIfComplete();

  // Returns the LowEnergyBluetooth adapter.
  BluetoothLowEnergyAdapterApple* GetLowEnergyAdapter() const;
  // Returns CBPeripheral.
  CBPeripheral* GetCBPeripheral() const;
  // Returns CBService.
  CBService* GetService() const;
  // Returns a remote characteristic based on the CBCharacteristic.
  BluetoothRemoteGattCharacteristicMac* GetBluetoothRemoteGattCharacteristicMac(
      CBCharacteristic* cb_characteristic) const;
  // Returns a remote descriptor based on the CBDescriptor.
  BluetoothRemoteGattDescriptorMac* GetBluetoothRemoteGattDescriptorMac(
      CBDescriptor* cb_descriptor) const;

  // bluetooth_device_mac_ owns instances of this class.
  raw_ptr<BluetoothLowEnergyDeviceMac> bluetooth_device_mac_;
  // A service from CBPeripheral.services.
  CBService* __strong service_;
  bool is_primary_;
  // Service identifier.
  std::string identifier_;
  // Service UUID.
  BluetoothUUID uuid_;
  // Increased each time DiscoverCharacteristics() is called. And decreased when
  // DidDiscoverCharacteristics() is called.
  int discovery_pending_count_;
};

// Stream operator for logging.
DEVICE_BLUETOOTH_EXPORT std::ostream& operator<<(
    std::ostream& out,
    const BluetoothRemoteGattServiceMac& service);

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_SERVICE_MAC_H_

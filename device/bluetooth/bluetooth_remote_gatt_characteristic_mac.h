// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_CHARACTERISTIC_MAC_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_CHARACTERISTIC_MAC_H_

#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"

#import <CoreBluetooth/CoreBluetooth.h>
#include <string>
#include <utility>
#include <vector>

#include "base/mac/scoped_nsobject.h"
#include "base/memory/weak_ptr.h"

namespace device {

class BluetoothAdapterMac;
class BluetoothRemoteGattDescriptorMac;
class BluetoothRemoteGattServiceMac;

// The BluetoothRemoteGattCharacteristicMac class implements
// BluetoothRemoteGattCharacteristic for remote GATT services on OS X.
class DEVICE_BLUETOOTH_EXPORT BluetoothRemoteGattCharacteristicMac
    : public BluetoothRemoteGattCharacteristic {
 public:
  BluetoothRemoteGattCharacteristicMac(
      BluetoothRemoteGattServiceMac* gatt_service,
      CBCharacteristic* cb_characteristic);
  ~BluetoothRemoteGattCharacteristicMac() override;

  // Override BluetoothGattCharacteristic methods.
  std::string GetIdentifier() const override;
  BluetoothUUID GetUUID() const override;
  Properties GetProperties() const override;
  Permissions GetPermissions() const override;

  // Override BluetoothRemoteGattCharacteristic methods.
  const std::vector<uint8_t>& GetValue() const override;
  BluetoothRemoteGattService* GetService() const override;
  bool IsNotifying() const override;
  void ReadRemoteCharacteristic(ValueCallback callback,
                                ErrorCallback error_callback) override;
  void WriteRemoteCharacteristic(const std::vector<uint8_t>& value,
                                 base::OnceClosure callback,
                                 ErrorCallback error_callback) override;
  bool WriteWithoutResponse(base::span<const uint8_t> value) override;

 protected:
  void SubscribeToNotifications(BluetoothRemoteGattDescriptor* ccc_descriptor,
                                base::OnceClosure callback,
                                ErrorCallback error_callback) override;
  void UnsubscribeFromNotifications(
      BluetoothRemoteGattDescriptor* ccc_descriptor,
      base::OnceClosure callback,
      ErrorCallback error_callback) override;

 private:
  friend class BluetoothLowEnergyDeviceMac;
  friend class BluetoothRemoteGattDescriptorMac;
  friend class BluetoothRemoteGattServiceMac;
  friend class BluetoothTestMac;

  void DiscoverDescriptors();
  // Called by the BluetoothRemoteGattServiceMac instance when the
  // characteristics value has been read.
  void DidUpdateValue(NSError* error);
  // Updates value_.
  void UpdateValue();
  // Called by the BluetoothRemoteGattServiceMac instance when the
  // characteristics value has been written.
  void DidWriteValue(NSError* error);
  // Called by the BluetoothRemoteGattServiceMac instance when the notify
  // session has been started or failed to be started.
  void DidUpdateNotificationState(NSError* error);
  // Called by the BluetoothRemoteGattServiceMac instance when the descriptors
  // has been discovered.
  void DidDiscoverDescriptors();
  // Returns true if the characteristic is readable.
  bool IsReadable() const;
  // Returns true if the characteristic is writable.
  bool IsWritable() const;
  // Returns true if the characteristic is writable without response.
  bool IsWritableWithoutResponse() const;
  // Returns true if the characteristic supports notifications or indications.
  bool SupportsNotificationsOrIndications() const;
  // Returns the write type (with or without responses).
  CBCharacteristicWriteType GetCBWriteType() const;
  // Returns CoreBluetooth characteristic.
  CBCharacteristic* GetCBCharacteristic() const;
  // Returns the mac adapter.
  BluetoothAdapterMac* GetMacAdapter() const;
  // Returns CoreBluetooth peripheral.
  CBPeripheral* GetCBPeripheral() const;
  // Returns true if this characteristic has been fully discovered.
  bool IsDiscoveryComplete() const;
  // Returns BluetoothRemoteGattDescriptorMac from CBDescriptor.
  BluetoothRemoteGattDescriptorMac* GetBluetoothRemoteGattDescriptorMac(
      CBDescriptor* cb_descriptor) const;
  bool HasPendingRead() const {
    return !read_characteristic_value_callbacks_.first.is_null();
  }
  bool HasPendingWrite() const {
    return !write_characteristic_value_callbacks_.first.is_null();
  }
  // Is true if the characteristic has been discovered with all its descriptors
  // and discovery_pending_count_ is 0.
  bool is_discovery_complete_;
  // Increased each time DiscoverDescriptors() is called. And decreased when
  // DidDiscoverDescriptors() is called.
  int discovery_pending_count_;
  // gatt_service_ owns instances of this class.
  BluetoothRemoteGattServiceMac* gatt_service_;
  // A characteristic from CBPeripheral.services.characteristics.
  base::scoped_nsobject<CBCharacteristic> cb_characteristic_;
  // Characteristic identifier.
  std::string identifier_;
  // Service UUID.
  BluetoothUUID uuid_;
  // Characteristic value.
  std::vector<uint8_t> value_;
  // ReadRemoteCharacteristic request callbacks.
  std::pair<ValueCallback, ErrorCallback> read_characteristic_value_callbacks_;
  // WriteRemoteCharacteristic request callbacks.
  std::pair<base::OnceClosure, ErrorCallback>
      write_characteristic_value_callbacks_;
  // Stores callbacks for SubscribeToNotifications and
  // UnsubscribeFromNotifications requests.
  typedef std::pair<base::OnceClosure, ErrorCallback> PendingNotifyCallbacks;
  // Stores SubscribeToNotifications request callbacks.
  PendingNotifyCallbacks subscribe_to_notification_callbacks_;
  // Stores UnsubscribeFromNotifications request callbacks.
  PendingNotifyCallbacks unsubscribe_from_notification_callbacks_;

  base::WeakPtrFactory<BluetoothRemoteGattCharacteristicMac> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothRemoteGattCharacteristicMac);
};

// Stream operator for logging.
DEVICE_BLUETOOTH_EXPORT std::ostream& operator<<(
    std::ostream& out,
    const BluetoothRemoteGattCharacteristicMac& characteristic);

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_CHARACTERISTIC_MAC_H_

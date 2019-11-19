// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_REMOTE_GATT_CHARACTERISTIC_BLUEZ_H_
#define DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_REMOTE_GATT_CHARACTERISTIC_BLUEZ_H_

#include <stddef.h>
#include <stdint.h>
#include <map>
#include <memory>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_remote_gatt_service.h"
#include "device/bluetooth/bluez/bluetooth_gatt_characteristic_bluez.h"
#include "device/bluetooth/dbus/bluetooth_gatt_descriptor_client.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

namespace device {

class BluetoothRemoteGattDescriptor;
class BluetoothRemoteGattService;

}  // namespace device

namespace bluez {

class BluetoothRemoteGattServiceBlueZ;

// The BluetoothRemoteGattCharacteristicBlueZ class implements
// BluetoothRemoteGattCharacteristic for remote GATT characteristics for
// platforms
// that use BlueZ.
class BluetoothRemoteGattCharacteristicBlueZ
    : public BluetoothGattCharacteristicBlueZ,
      public BluetoothGattDescriptorClient::Observer,
      public device::BluetoothRemoteGattCharacteristic {
 public:
  // device::BluetoothGattCharacteristic overrides.
  ~BluetoothRemoteGattCharacteristicBlueZ() override;
  device::BluetoothUUID GetUUID() const override;
  Properties GetProperties() const override;
  Permissions GetPermissions() const override;

  // device::BluetoothRemoteGattCharacteristic overrides.
  const std::vector<uint8_t>& GetValue() const override;
  device::BluetoothRemoteGattService* GetService() const override;
  bool IsNotifying() const override;
  void ReadRemoteCharacteristic(ValueCallback callback,
                                ErrorCallback error_callback) override;
  void WriteRemoteCharacteristic(const std::vector<uint8_t>& value,
                                 base::OnceClosure callback,
                                 ErrorCallback error_callback) override;
#if defined(OS_CHROMEOS)
  void PrepareWriteRemoteCharacteristic(const std::vector<uint8_t>& value,
                                        base::OnceClosure callback,
                                        ErrorCallback error_callback) override;
#endif

 protected:
#if defined(OS_CHROMEOS)
  void SubscribeToNotifications(
      device::BluetoothRemoteGattDescriptor* ccc_descriptor,
      NotificationType notification_type,
      base::OnceClosure callback,
      ErrorCallback error_callback) override;
#else
  void SubscribeToNotifications(
      device::BluetoothRemoteGattDescriptor* ccc_descriptor,
      base::OnceClosure callback,
      ErrorCallback error_callback) override;
#endif
  void UnsubscribeFromNotifications(
      device::BluetoothRemoteGattDescriptor* ccc_descriptor,
      base::OnceClosure callback,
      ErrorCallback error_callback) override;

 private:
  friend class BluetoothRemoteGattServiceBlueZ;

  BluetoothRemoteGattCharacteristicBlueZ(
      BluetoothRemoteGattServiceBlueZ* service,
      const dbus::ObjectPath& object_path);

  // bluez::BluetoothGattDescriptorClient::Observer overrides.
  void GattDescriptorAdded(const dbus::ObjectPath& object_path) override;
  void GattDescriptorRemoved(const dbus::ObjectPath& object_path) override;
  void GattDescriptorPropertyChanged(const dbus::ObjectPath& object_path,
                                     const std::string& property_name) override;

  // Called by dbus:: on successful completion of a request to start
  // notifications.
  void OnStartNotifySuccess(base::OnceClosure callback);

  // Called by dbus:: on unsuccessful completion of a request to start
  // notifications.
  void OnStartNotifyError(ErrorCallback error_callback,
                          const std::string& error_name,
                          const std::string& error_message);

  // Called by dbus:: on successful completion of a request to stop
  // notifications.
  void OnStopNotifySuccess(base::OnceClosure callback);

  // Called by dbus:: on unsuccessful completion of a request to stop
  // notifications.
  void OnStopNotifyError(base::OnceClosure callback,
                         const std::string& error_name,
                         const std::string& error_message);

  // Called by dbus:: on unsuccessful completion of a request to read
  // the characteristic value.
  void OnReadError(ErrorCallback error_callback,
                   const std::string& error_name,
                   const std::string& error_message);

  // Called by dbus:: on unsuccessful completion of a request to write
  // the characteristic value.
  void OnWriteError(ErrorCallback error_callback,
                    const std::string& error_name,
                    const std::string& error_message);

  // True, if there exists a Bluez notify session.
  bool has_notify_session_;

  // The GATT service this GATT characteristic belongs to.
  BluetoothRemoteGattServiceBlueZ* service_;

  // Number of gatt read requests in progress.
  int num_of_characteristic_value_read_in_progress_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothRemoteGattCharacteristicBlueZ>
      weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BluetoothRemoteGattCharacteristicBlueZ);
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_REMOTE_GATT_CHARACTERISTIC_BLUEZ_H_

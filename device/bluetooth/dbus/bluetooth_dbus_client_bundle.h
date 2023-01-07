// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_DBUS_BLUETOOTH_DBUS_CLIENT_BUNDLE_H_
#define DEVICE_BLUETOOTH_DBUS_BLUETOOTH_DBUS_CLIENT_BUNDLE_H_

#include <memory>
#include <string>

#include "device/bluetooth/bluetooth_export.h"

namespace bluez {

class BluetoothAdapterClient;
class BluetoothAdminPolicyClient;
class BluetoothAdvertisementMonitorManagerClient;
class BluetoothAgentManagerClient;
class BluetoothBatteryClient;
class BluetoothDebugManagerClient;
class BluetoothDeviceClient;
class BluetoothGattCharacteristicClient;
class BluetoothGattDescriptorClient;
class BluetoothGattManagerClient;
class BluetoothGattServiceClient;
class BluetoothInputClient;
class BluetoothLEAdvertisingManagerClient;
class BluetoothProfileManagerClient;

// The bundle of all D-Bus clients used in DBusThreadManager. The bundle
// is used to delete them at once in the right order before shutting down the
// system bus. See also the comment in the destructor of DBusThreadManager.
class DEVICE_BLUETOOTH_EXPORT BluetoothDBusClientBundle {
 public:
  explicit BluetoothDBusClientBundle(bool use_fakes);

  BluetoothDBusClientBundle(const BluetoothDBusClientBundle&) = delete;
  BluetoothDBusClientBundle& operator=(const BluetoothDBusClientBundle&) =
      delete;

  ~BluetoothDBusClientBundle();

  // Returns true if |client| is stubbed.
  bool IsUsingFakes() { return use_fakes_; }

  BluetoothAdapterClient* bluetooth_adapter_client() {
    return bluetooth_adapter_client_.get();
  }

  BluetoothAdminPolicyClient* bluetooth_admin_policy_client() {
    return bluetooth_admin_policy_client_.get();
  }

  BluetoothLEAdvertisingManagerClient*
  bluetooth_le_advertising_manager_client() {
    return bluetooth_le_advertising_manager_client_.get();
  }

  BluetoothAdvertisementMonitorManagerClient*
  bluetooth_advertisement_monitor_manager_client() {
    return bluetooth_advertisement_monitor_manager_client_.get();
  }

  BluetoothAgentManagerClient* bluetooth_agent_manager_client() {
    return bluetooth_agent_manager_client_.get();
  }

  BluetoothDebugManagerClient* bluetooth_debug_manager_client() {
    return bluetooth_debug_manager_client_.get();
  }

  BluetoothBatteryClient* bluetooth_battery_client() {
    return bluetooth_battery_client_.get();
  }

  BluetoothDeviceClient* bluetooth_device_client() {
    return bluetooth_device_client_.get();
  }

  BluetoothGattCharacteristicClient* bluetooth_gatt_characteristic_client() {
    return bluetooth_gatt_characteristic_client_.get();
  }

  BluetoothGattDescriptorClient* bluetooth_gatt_descriptor_client() {
    return bluetooth_gatt_descriptor_client_.get();
  }

  BluetoothGattManagerClient* bluetooth_gatt_manager_client() {
    return bluetooth_gatt_manager_client_.get();
  }

  BluetoothGattServiceClient* bluetooth_gatt_service_client() {
    return bluetooth_gatt_service_client_.get();
  }

  BluetoothInputClient* bluetooth_input_client() {
    return bluetooth_input_client_.get();
  }

  BluetoothProfileManagerClient* bluetooth_profile_manager_client() {
    return bluetooth_profile_manager_client_.get();
  }

  BluetoothAdapterClient* alternate_bluetooth_adapter_client() {
    return alternate_bluetooth_adapter_client_.get();
  }

  BluetoothAdminPolicyClient* alternate_bluetooth_admin_policy_client() {
    return alternate_bluetooth_admin_policy_client_.get();
  }

  BluetoothDeviceClient* alternate_bluetooth_device_client() {
    return alternate_bluetooth_device_client_.get();
  }

 private:
  friend class BluezDBusManagerSetter;

  bool use_fakes_;

  std::unique_ptr<BluetoothAdapterClient> bluetooth_adapter_client_;
  std::unique_ptr<BluetoothAdminPolicyClient> bluetooth_admin_policy_client_;
  std::unique_ptr<BluetoothAdvertisementMonitorManagerClient>
      bluetooth_advertisement_monitor_manager_client_;
  std::unique_ptr<BluetoothLEAdvertisingManagerClient>
      bluetooth_le_advertising_manager_client_;
  std::unique_ptr<BluetoothAgentManagerClient> bluetooth_agent_manager_client_;
  std::unique_ptr<BluetoothBatteryClient> bluetooth_battery_client_;
  std::unique_ptr<BluetoothDebugManagerClient> bluetooth_debug_manager_client_;
  std::unique_ptr<BluetoothDeviceClient> bluetooth_device_client_;
  std::unique_ptr<BluetoothGattCharacteristicClient>
      bluetooth_gatt_characteristic_client_;
  std::unique_ptr<BluetoothGattDescriptorClient>
      bluetooth_gatt_descriptor_client_;
  std::unique_ptr<BluetoothGattManagerClient> bluetooth_gatt_manager_client_;
  std::unique_ptr<BluetoothGattServiceClient> bluetooth_gatt_service_client_;
  std::unique_ptr<BluetoothInputClient> bluetooth_input_client_;
  std::unique_ptr<BluetoothProfileManagerClient>
      bluetooth_profile_manager_client_;

  // See "Alternate D-Bus Client" note in bluez_dbus_manager.h.
  std::unique_ptr<BluetoothAdapterClient> alternate_bluetooth_adapter_client_;
  std::unique_ptr<BluetoothAdminPolicyClient>
      alternate_bluetooth_admin_policy_client_;
  std::unique_ptr<BluetoothDeviceClient> alternate_bluetooth_device_client_;
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_DBUS_BLUETOOTH_DBUS_CLIENT_BUNDLE_H_

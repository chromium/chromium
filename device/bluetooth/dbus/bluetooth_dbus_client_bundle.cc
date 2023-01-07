// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/bluetooth_dbus_client_bundle.h"

#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chromeos/constants/chromeos_features.h"
#include "device/bluetooth/dbus/bluetooth_adapter_client.h"
#include "device/bluetooth/dbus/bluetooth_admin_policy_client.h"
#include "device/bluetooth/dbus/bluetooth_advertisement_monitor_manager_client.h"
#include "device/bluetooth/dbus/bluetooth_agent_manager_client.h"
#include "device/bluetooth/dbus/bluetooth_battery_client.h"
#include "device/bluetooth/dbus/bluetooth_debug_manager_client.h"
#include "device/bluetooth/dbus/bluetooth_device_client.h"
#include "device/bluetooth/dbus/bluetooth_gatt_characteristic_client.h"
#include "device/bluetooth/dbus/bluetooth_gatt_descriptor_client.h"
#include "device/bluetooth/dbus/bluetooth_gatt_manager_client.h"
#include "device/bluetooth/dbus/bluetooth_gatt_service_client.h"
#include "device/bluetooth/dbus/bluetooth_input_client.h"
#include "device/bluetooth/dbus/bluetooth_le_advertising_manager_client.h"
#include "device/bluetooth/dbus/bluetooth_profile_manager_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_adapter_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_admin_policy_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_advertisement_monitor_manager_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_agent_manager_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_battery_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_debug_manager_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_device_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_gatt_characteristic_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_gatt_descriptor_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_gatt_manager_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_gatt_service_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_input_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_le_advertising_manager_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_profile_manager_client.h"

namespace bluez {

BluetoothDBusClientBundle::BluetoothDBusClientBundle(bool use_fakes)
    : use_fakes_(use_fakes) {
  if (!use_fakes_) {
    bluetooth_adapter_client_.reset(BluetoothAdapterClient::Create());
    bluetooth_admin_policy_client_ = BluetoothAdminPolicyClient::Create();
    bluetooth_le_advertising_manager_client_.reset(
        BluetoothLEAdvertisingManagerClient::Create());
#if BUILDFLAG(IS_CHROMEOS)
    bluetooth_advertisement_monitor_manager_client_ =
        BluetoothAdvertisementMonitorManagerClient::Create();
#endif  // BUILDFLAG(IS_CHROMEOS)
    bluetooth_agent_manager_client_.reset(
        BluetoothAgentManagerClient::Create());
    bluetooth_battery_client_.reset(BluetoothBatteryClient::Create());
    bluetooth_debug_manager_client_.reset(
        BluetoothDebugManagerClient::Create());
    bluetooth_device_client_.reset(BluetoothDeviceClient::Create());
    bluetooth_input_client_.reset(BluetoothInputClient::Create());
    bluetooth_profile_manager_client_.reset(
        BluetoothProfileManagerClient::Create());
    bluetooth_gatt_characteristic_client_.reset(
        BluetoothGattCharacteristicClient::Create());
    bluetooth_gatt_descriptor_client_.reset(
        BluetoothGattDescriptorClient::Create());
    bluetooth_gatt_manager_client_.reset(BluetoothGattManagerClient::Create());
    bluetooth_gatt_service_client_.reset(BluetoothGattServiceClient::Create());

    alternate_bluetooth_adapter_client_.reset(BluetoothAdapterClient::Create());
    alternate_bluetooth_admin_policy_client_ =
        BluetoothAdminPolicyClient::Create();
    alternate_bluetooth_device_client_.reset(BluetoothDeviceClient::Create());
  } else {
#if defined(USE_REAL_DBUS_CLIENTS)
    LOG(FATAL) << "Fakes are unavailable if USE_REAL_DBUS_CLIENTS is defined.";
#else
    bluetooth_adapter_client_ = std::make_unique<FakeBluetoothAdapterClient>();
    bluetooth_admin_policy_client_ =
        std::make_unique<FakeBluetoothAdminPolicyClient>();
    bluetooth_le_advertising_manager_client_ =
        std::make_unique<FakeBluetoothLEAdvertisingManagerClient>();
#if BUILDFLAG(IS_CHROMEOS)
    bluetooth_advertisement_monitor_manager_client_ =
        std::make_unique<FakeBluetoothAdvertisementMonitorManagerClient>();
#endif  // BUILDFLAG(IS_CHROMEOS)
    bluetooth_agent_manager_client_ =
        std::make_unique<FakeBluetoothAgentManagerClient>();
    bluetooth_battery_client_ = std::make_unique<FakeBluetoothBatteryClient>();
    bluetooth_debug_manager_client_ =
        std::make_unique<FakeBluetoothDebugManagerClient>();
    bluetooth_device_client_ = std::make_unique<FakeBluetoothDeviceClient>();
    bluetooth_input_client_ = std::make_unique<FakeBluetoothInputClient>();
    bluetooth_profile_manager_client_ =
        std::make_unique<FakeBluetoothProfileManagerClient>();
    bluetooth_gatt_characteristic_client_ =
        std::make_unique<FakeBluetoothGattCharacteristicClient>();
    bluetooth_gatt_descriptor_client_ =
        std::make_unique<FakeBluetoothGattDescriptorClient>();
    bluetooth_gatt_manager_client_ =
        std::make_unique<FakeBluetoothGattManagerClient>();
    bluetooth_gatt_service_client_ =
        std::make_unique<FakeBluetoothGattServiceClient>();

    alternate_bluetooth_adapter_client_ =
        std::make_unique<FakeBluetoothAdapterClient>();
    alternate_bluetooth_admin_policy_client_ =
        std::make_unique<FakeBluetoothAdminPolicyClient>();
    alternate_bluetooth_device_client_ =
        std::make_unique<FakeBluetoothDeviceClient>();
#endif  // defined(USE_REAL_DBUS_CLIENTS)
  }
}

BluetoothDBusClientBundle::~BluetoothDBusClientBundle() = default;

}  // namespace bluez

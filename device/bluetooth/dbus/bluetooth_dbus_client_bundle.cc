// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/bluetooth_dbus_client_bundle.h"

#include <vector>

#include "base/command_line.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "device/bluetooth/dbus/bluetooth_adapter_client.h"
#include "device/bluetooth/dbus/bluetooth_agent_manager_client.h"
#include "device/bluetooth/dbus/bluetooth_device_client.h"
#include "device/bluetooth/dbus/bluetooth_gatt_characteristic_client.h"
#include "device/bluetooth/dbus/bluetooth_gatt_descriptor_client.h"
#include "device/bluetooth/dbus/bluetooth_gatt_manager_client.h"
#include "device/bluetooth/dbus/bluetooth_gatt_service_client.h"
#include "device/bluetooth/dbus/bluetooth_input_client.h"
#include "device/bluetooth/dbus/bluetooth_le_advertising_manager_client.h"
#include "device/bluetooth/dbus/bluetooth_media_client.h"
#include "device/bluetooth/dbus/bluetooth_media_transport_client.h"
#include "device/bluetooth/dbus/bluetooth_profile_manager_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_adapter_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_agent_manager_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_device_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_gatt_characteristic_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_gatt_descriptor_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_gatt_manager_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_gatt_service_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_input_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_le_advertising_manager_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_media_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_media_transport_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_profile_manager_client.h"

namespace bluez {

BluetoothDBusClientBundle::BluetoothDBusClientBundle(bool use_fakes)
    : use_fakes_(use_fakes) {
  if (!use_fakes_) {
    bluetooth_adapter_client_.reset(BluetoothAdapterClient::Create());
    bluetooth_le_advertising_manager_client_.reset(
        BluetoothLEAdvertisingManagerClient::Create());
    bluetooth_agent_manager_client_.reset(
        BluetoothAgentManagerClient::Create());
    bluetooth_device_client_.reset(BluetoothDeviceClient::Create());
    bluetooth_input_client_.reset(BluetoothInputClient::Create());
    bluetooth_media_client_.reset(BluetoothMediaClient::Create());
    bluetooth_media_transport_client_.reset(
        BluetoothMediaTransportClient::Create());
    bluetooth_profile_manager_client_.reset(
        BluetoothProfileManagerClient::Create());
    bluetooth_gatt_characteristic_client_.reset(
        BluetoothGattCharacteristicClient::Create());
    bluetooth_gatt_descriptor_client_.reset(
        BluetoothGattDescriptorClient::Create());
    bluetooth_gatt_manager_client_.reset(BluetoothGattManagerClient::Create());
    bluetooth_gatt_service_client_.reset(BluetoothGattServiceClient::Create());

    alternate_bluetooth_adapter_client_.reset(BluetoothAdapterClient::Create());
  } else {
    bluetooth_adapter_client_.reset(new FakeBluetoothAdapterClient);
    bluetooth_le_advertising_manager_client_.reset(
        new FakeBluetoothLEAdvertisingManagerClient);
    bluetooth_agent_manager_client_.reset(new FakeBluetoothAgentManagerClient);
    bluetooth_device_client_.reset(new FakeBluetoothDeviceClient);
    bluetooth_input_client_.reset(new FakeBluetoothInputClient);
    bluetooth_media_client_.reset(new FakeBluetoothMediaClient);
    bluetooth_media_transport_client_.reset(
        new FakeBluetoothMediaTransportClient);
    bluetooth_profile_manager_client_.reset(
        new FakeBluetoothProfileManagerClient);
    bluetooth_gatt_characteristic_client_.reset(
        new FakeBluetoothGattCharacteristicClient);
    bluetooth_gatt_descriptor_client_.reset(
        new FakeBluetoothGattDescriptorClient);
    bluetooth_gatt_manager_client_.reset(new FakeBluetoothGattManagerClient);
    bluetooth_gatt_service_client_.reset(new FakeBluetoothGattServiceClient);

    alternate_bluetooth_adapter_client_.reset(new FakeBluetoothAdapterClient);
  }
}

BluetoothDBusClientBundle::~BluetoothDBusClientBundle() = default;

}  // namespace bluez

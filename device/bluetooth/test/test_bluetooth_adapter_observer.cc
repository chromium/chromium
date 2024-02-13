// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/test_bluetooth_adapter_observer.h"

#include <string>
#include <vector>

#include "base/run_loop.h"
#include "build/build_config.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_remote_gatt_descriptor.h"
#include "device/bluetooth/bluetooth_remote_gatt_service.h"
#include "test_bluetooth_adapter_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

TestBluetoothAdapterObserver::TestBluetoothAdapterObserver(
    scoped_refptr<BluetoothAdapter> adapter)
    : adapter_(adapter) {
  Reset();
  adapter_->AddObserver(this);
}

TestBluetoothAdapterObserver::~TestBluetoothAdapterObserver() {
  adapter_->RemoveObserver(this);
}

void TestBluetoothAdapterObserver::Reset() {
  present_changed_count_ = 0;
  powered_changed_count_ = 0;
  discoverable_changed_count_ = 0;
  discovering_changed_count_ = 0;
  last_present_ = false;
  last_powered_ = false;
  last_discovering_ = false;
  device_added_count_ = 0;
  device_changed_count_ = 0;
  device_address_changed_count_ = 0;
  device_advertisement_raw_received_count_ = 0;
  last_device_name_ = "";
  last_advertisement_name_ = "";
  last_rssi_ = 128;
  last_tx_power_ = 128;
  last_appearance_ = 128;
#if BUILDFLAG(IS_CHROMEOS)
  device_bonded_changed_count_ = 0;
  device_new_bonded_status_ = false;
#endif
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  device_paired_changed_count_ = 0;
  device_new_paired_status_ = false;
  device_mtu_changed_count_ = 0;
  device_mtu_ = 0;
  device_advertisement_received_count_ = 0;
  device_eir_.clear();
  device_connected_state_changed_values_.clear();
#endif
  device_removed_count_ = 0;
  last_device_ = nullptr;
  last_device_address_.clear();
  gatt_service_added_count_ = 0;
  gatt_service_removed_count_ = 0;
  gatt_services_discovered_count_ = 0;
  gatt_service_changed_count_ = 0;
  gatt_discovery_complete_count_ = 0;
  gatt_characteristic_added_count_ = 0;
  gatt_characteristic_removed_count_ = 0;
  gatt_characteristic_value_changed_count_ = 0;
  gatt_descriptor_added_count_ = 0;
  gatt_descriptor_removed_count_ = 0;
  gatt_descriptor_value_changed_count_ = 0;
  last_gatt_service_id_.clear();
  last_gatt_service_uuid_ = BluetoothUUID();
  last_gatt_characteristic_id_.clear();
  last_gatt_characteristic_uuid_ = BluetoothUUID();
  last_changed_characteristic_value_.clear();
  previous_characteristic_value_changed_values_.clear();
  last_gatt_descriptor_id_.clear();
  last_gatt_descriptor_uuid_ = BluetoothUUID();
  last_changed_descriptor_value_.clear();
}

void TestBluetoothAdapterObserver::set_quit_closure(
    base::OnceClosure quit_closure) {
  quit_closure_ = std::move(quit_closure);
}

void TestBluetoothAdapterObserver::AdapterPresentChanged(
    BluetoothAdapter* adapter,
    bool present) {
  EXPECT_EQ(adapter_.get(), adapter);

  ++present_changed_count_;
  last_present_ = present;
}

void TestBluetoothAdapterObserver::AdapterPoweredChanged(
    BluetoothAdapter* adapter,
    bool powered) {
  EXPECT_EQ(adapter_.get(), adapter);

  ++powered_changed_count_;
  last_powered_ = powered;
}

void TestBluetoothAdapterObserver::AdapterDiscoverableChanged(
    BluetoothAdapter* adapter,
    bool discoverable) {
  EXPECT_EQ(adapter_.get(), adapter);

  ++discoverable_changed_count_;
}

void TestBluetoothAdapterObserver::AdapterDiscoveringChanged(
    BluetoothAdapter* adapter,
    bool discovering) {
  EXPECT_EQ(adapter_.get(), adapter);

  ++discovering_changed_count_;
  last_discovering_ = discovering;
  if (discovering_changed_callback_)
    discovering_changed_callback_.Run();
}

void TestBluetoothAdapterObserver::RegisterDiscoveringChangedWatcher(
    base::RepeatingClosure callback) {
  discovering_changed_callback_ = callback;
}

void TestBluetoothAdapterObserver::RegisterDiscoveryChangeCompletedWatcher(
    base::RepeatingClosure callback) {
  discovery_change_completed_callback_ = callback;
}

void TestBluetoothAdapterObserver::DeviceAdded(BluetoothAdapter* adapter,
                                               BluetoothDevice* device) {
  EXPECT_EQ(adapter_.get(), adapter);

  ++device_added_count_;
  last_device_ = device;
  last_device_address_ = device->GetAddress();

  QuitMessageLoop();
}

void TestBluetoothAdapterObserver::DiscoveryChangeCompletedForTesting() {
  if (discovery_change_completed_callback_)
    discovery_change_completed_callback_.Run();
}

void TestBluetoothAdapterObserver::DeviceChanged(BluetoothAdapter* adapter,
                                                 BluetoothDevice* device) {
  EXPECT_EQ(adapter_.get(), adapter);

  ++device_changed_count_;
  last_device_ = device;
  last_device_address_ = device->GetAddress();

  QuitMessageLoop();
}

void TestBluetoothAdapterObserver::DeviceAddressChanged(
    device::BluetoothAdapter* adapter,
    device::BluetoothDevice* device,
    const std::string& old_address) {
  ++device_address_changed_count_;
  last_device_ = device;
  last_device_address_ = device->GetAddress();

  QuitMessageLoop();
}

void TestBluetoothAdapterObserver::DeviceAdvertisementReceived(
    const std::string& device_address,
    const std::optional<std::string>& device_name,
    const std::optional<std::string>& advertisement_name,
    std::optional<int8_t> rssi,
    std::optional<int8_t> tx_power,
    std::optional<uint16_t> appearance,
    const device::BluetoothDevice::UUIDList& advertised_uuids,
    const device::BluetoothDevice::ServiceDataMap& service_data_map,
    const device::BluetoothDevice::ManufacturerDataMap& manufacturer_data_map) {
  ++device_advertisement_raw_received_count_;
  last_device_address_ = device_address;
  last_device_name_ = device_name;
  last_advertisement_name_ = advertisement_name;
  last_rssi_ = rssi;
  last_tx_power_ = tx_power;
  last_appearance_ = appearance;
  last_advertised_uuids_ = advertised_uuids;
  last_service_data_map_ = service_data_map;
  last_manufacturer_data_map_ = manufacturer_data_map;

  QuitMessageLoop();
}

#if BUILDFLAG(IS_CHROMEOS)
void TestBluetoothAdapterObserver::DeviceBondedChanged(
    device::BluetoothAdapter* adapter,
    device::BluetoothDevice* device,
    bool new_bonded_status) {
  ++device_bonded_changed_count_;
  last_device_ = device;
  device_new_bonded_status_ = new_bonded_status;

  QuitMessageLoop();
}
#endif

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
void TestBluetoothAdapterObserver::DevicePairedChanged(
    device::BluetoothAdapter* adapter,
    device::BluetoothDevice* device,
    bool new_paired_status) {
  ++device_paired_changed_count_;
  last_device_ = device;
  device_new_paired_status_ = new_paired_status;

  QuitMessageLoop();
}

void TestBluetoothAdapterObserver::DeviceMTUChanged(
    device::BluetoothAdapter* adapter,
    device::BluetoothDevice* device,
    uint16_t mtu) {
  ++device_mtu_changed_count_;
  last_device_ = device;
  device_mtu_ = mtu;

  QuitMessageLoop();
}

void TestBluetoothAdapterObserver::DeviceAdvertisementReceived(
    device::BluetoothAdapter* adapter,
    device::BluetoothDevice* device,
    int16_t rssi,
    const std::vector<uint8_t>& eir) {
  ++device_advertisement_received_count_;
  last_device_ = device;
  device_eir_ = eir;

  QuitMessageLoop();
}

void TestBluetoothAdapterObserver::DeviceConnectedStateChanged(
    device::BluetoothAdapter* adapter,
    device::BluetoothDevice* device,
    bool is_now_connected) {
  last_device_ = device;
  device_connected_state_changed_values_.push_back(is_now_connected);

  QuitMessageLoop();
}
#endif

void TestBluetoothAdapterObserver::DeviceRemoved(BluetoothAdapter* adapter,
                                                 BluetoothDevice* device) {
  EXPECT_EQ(adapter_.get(), adapter);

  ++device_removed_count_;
  // Can't save device, it may be freed
  last_device_address_ = device->GetAddress();

  QuitMessageLoop();
}

void TestBluetoothAdapterObserver::GattServiceAdded(
    BluetoothAdapter* adapter,
    BluetoothDevice* device,
    BluetoothRemoteGattService* service) {
  ASSERT_EQ(adapter_.get(), adapter);
  ASSERT_EQ(service->GetDevice(), device);

  ++gatt_service_added_count_;
  last_gatt_service_id_ = service->GetIdentifier();
  last_gatt_service_uuid_ = service->GetUUID();

  EXPECT_TRUE(service->IsPrimary());

  EXPECT_EQ(device->GetGattService(last_gatt_service_id_), service);

  QuitMessageLoop();
}

void TestBluetoothAdapterObserver::GattServiceRemoved(
    BluetoothAdapter* adapter,
    BluetoothDevice* device,
    BluetoothRemoteGattService* service) {
  ASSERT_EQ(adapter_.get(), adapter);
  ASSERT_EQ(service->GetDevice(), device);

  ++gatt_service_removed_count_;
  last_gatt_service_id_ = service->GetIdentifier();
  last_gatt_service_uuid_ = service->GetUUID();

  EXPECT_TRUE(service->IsPrimary());

  // The device should return NULL for this service.
  EXPECT_FALSE(device->GetGattService(last_gatt_service_id_));

  QuitMessageLoop();
}

void TestBluetoothAdapterObserver::GattServicesDiscovered(
    BluetoothAdapter* adapter,
    BluetoothDevice* device) {
  ASSERT_EQ(adapter_.get(), adapter);

  ++gatt_services_discovered_count_;
  last_device_ = device;
  last_device_address_ = device->GetAddress();

  QuitMessageLoop();
}

void TestBluetoothAdapterObserver::GattDiscoveryCompleteForService(
    BluetoothAdapter* adapter,
    BluetoothRemoteGattService* service) {
  ASSERT_EQ(adapter_.get(), adapter);
  ++gatt_discovery_complete_count_;

  QuitMessageLoop();
}

void TestBluetoothAdapterObserver::GattServiceChanged(
    BluetoothAdapter* adapter,
    BluetoothRemoteGattService* service) {
  ASSERT_EQ(adapter_.get(), adapter);
  ++gatt_service_changed_count_;

  QuitMessageLoop();
}

void TestBluetoothAdapterObserver::GattCharacteristicAdded(
    BluetoothAdapter* adapter,
    BluetoothRemoteGattCharacteristic* characteristic) {
  ASSERT_EQ(adapter_.get(), adapter);

  ++gatt_characteristic_added_count_;
  last_gatt_characteristic_id_ = characteristic->GetIdentifier();
  last_gatt_characteristic_uuid_ = characteristic->GetUUID();

  ASSERT_TRUE(characteristic->GetService());
  EXPECT_EQ(characteristic->GetService()->GetCharacteristic(
                last_gatt_characteristic_id_),
            characteristic);

  QuitMessageLoop();
}

void TestBluetoothAdapterObserver::GattCharacteristicRemoved(
    BluetoothAdapter* adapter,
    BluetoothRemoteGattCharacteristic* characteristic) {
  ASSERT_EQ(adapter_.get(), adapter);

  ++gatt_characteristic_removed_count_;
  last_gatt_characteristic_id_ = characteristic->GetIdentifier();
  last_gatt_characteristic_uuid_ = characteristic->GetUUID();

  // The service should return NULL for this characteristic.
  ASSERT_TRUE(characteristic->GetService());
  EXPECT_FALSE(characteristic->GetService()->GetCharacteristic(
      last_gatt_characteristic_id_));

  QuitMessageLoop();
}

void TestBluetoothAdapterObserver::GattDescriptorAdded(
    BluetoothAdapter* adapter,
    BluetoothRemoteGattDescriptor* descriptor) {
  ASSERT_EQ(adapter_.get(), adapter);

  ++gatt_descriptor_added_count_;
  last_gatt_descriptor_id_ = descriptor->GetIdentifier();
  last_gatt_descriptor_uuid_ = descriptor->GetUUID();

  ASSERT_TRUE(descriptor->GetCharacteristic());
  EXPECT_EQ(
      descriptor->GetCharacteristic()->GetDescriptor(last_gatt_descriptor_id_),
      descriptor);

  QuitMessageLoop();
}

void TestBluetoothAdapterObserver::GattDescriptorRemoved(
    BluetoothAdapter* adapter,
    BluetoothRemoteGattDescriptor* descriptor) {
  ASSERT_EQ(adapter_.get(), adapter);

  ++gatt_descriptor_removed_count_;
  last_gatt_descriptor_id_ = descriptor->GetIdentifier();
  last_gatt_descriptor_uuid_ = descriptor->GetUUID();

  // The characteristic should return NULL for this descriptor..
  ASSERT_TRUE(descriptor->GetCharacteristic());
  EXPECT_FALSE(
      descriptor->GetCharacteristic()->GetDescriptor(last_gatt_descriptor_id_));

  QuitMessageLoop();
}

void TestBluetoothAdapterObserver::GattCharacteristicValueChanged(
    BluetoothAdapter* adapter,
    BluetoothRemoteGattCharacteristic* characteristic,
    const std::vector<uint8_t>& value) {
  ASSERT_EQ(adapter_.get(), adapter);

  ++gatt_characteristic_value_changed_count_;
  last_gatt_characteristic_id_ = characteristic->GetIdentifier();
  last_gatt_characteristic_uuid_ = characteristic->GetUUID();
  last_changed_characteristic_value_ = value;
  previous_characteristic_value_changed_values_.push_back(value);

  ASSERT_TRUE(characteristic->GetService());
  EXPECT_EQ(characteristic->GetService()->GetCharacteristic(
                last_gatt_characteristic_id_),
            characteristic);

  QuitMessageLoop();
}

void TestBluetoothAdapterObserver::GattDescriptorValueChanged(
    BluetoothAdapter* adapter,
    BluetoothRemoteGattDescriptor* descriptor,
    const std::vector<uint8_t>& value) {
  ASSERT_EQ(adapter_.get(), adapter);

  ++gatt_descriptor_value_changed_count_;
  last_gatt_descriptor_id_ = descriptor->GetIdentifier();
  last_gatt_descriptor_uuid_ = descriptor->GetUUID();
  last_changed_descriptor_value_ = value;

  ASSERT_TRUE(descriptor->GetCharacteristic());
  EXPECT_EQ(
      descriptor->GetCharacteristic()->GetDescriptor(last_gatt_descriptor_id_),
      descriptor);

  QuitMessageLoop();
}

#if BUILDFLAG(IS_CHROMEOS)
void TestBluetoothAdapterObserver::
    LowEnergyScanSessionHardwareOffloadingStatusChanged(
        BluetoothAdapter::LowEnergyScanSessionHardwareOffloadingStatus status) {
  last_low_energy_scan_session_hardware_offloading_status_ = status;
  QuitMessageLoop();
}
#endif  // BUILDFLAG(IS_CHROMEOS)

void TestBluetoothAdapterObserver::QuitMessageLoop() {
  // only call closure if set
  if (!quit_closure_.is_null()) {
    std::move(quit_closure_).Run();
  }
}

}  // namespace device

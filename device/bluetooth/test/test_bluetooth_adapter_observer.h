// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_TEST_BLUETOOTH_ADAPTER_OBSERVER_H_
#define DEVICE_BLUETOOTH_TEST_TEST_BLUETOOTH_ADAPTER_OBSERVER_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace device {

// Test implementation of BluetoothAdapter::Observer counting method calls and
// caching last reported values.
class TestBluetoothAdapterObserver : public BluetoothAdapter::Observer {
 public:
  explicit TestBluetoothAdapterObserver(
      scoped_refptr<BluetoothAdapter> adapter);

  TestBluetoothAdapterObserver(const TestBluetoothAdapterObserver&) = delete;
  TestBluetoothAdapterObserver& operator=(const TestBluetoothAdapterObserver&) =
      delete;

  ~TestBluetoothAdapterObserver() override;

  // Reset counters and cached values.
  void Reset();

  void set_quit_closure(base::OnceClosure quit_closure);

  // BluetoothAdapter::Observer
  void AdapterPresentChanged(BluetoothAdapter* adapter, bool present) override;
  void AdapterPoweredChanged(BluetoothAdapter* adapter, bool powered) override;
  void AdapterDiscoverableChanged(BluetoothAdapter* adapter,
                                  bool discoverable) override;
  void AdapterDiscoveringChanged(BluetoothAdapter* adapter,
                                 bool discovering) override;
  void RegisterDiscoveringChangedWatcher(base::RepeatingClosure callback);
  void RegisterDiscoveryChangeCompletedWatcher(base::RepeatingClosure callback);
  void DeviceAdded(BluetoothAdapter* adapter, BluetoothDevice* device) override;
  void DiscoveryChangeCompletedForTesting() override;
  void DeviceChanged(BluetoothAdapter* adapter,
                     BluetoothDevice* device) override;
  void DeviceAddressChanged(device::BluetoothAdapter* adapter,
                            device::BluetoothDevice* device,
                            const std::string& old_address) override;
  void DeviceAdvertisementReceived(
      const std::string& device_id,
      const std::optional<std::string>& device_name,
      const std::optional<std::string>& advertisement_name,
      std::optional<int8_t> rssi,
      std::optional<int8_t> tx_power,
      std::optional<uint16_t> appearance,
      const device::BluetoothDevice::UUIDList& advertised_uuids,
      const device::BluetoothDevice::ServiceDataMap& service_data_map,
      const device::BluetoothDevice::ManufacturerDataMap& manufacturer_data_map)
      override;
#if BUILDFLAG(IS_CHROMEOS)
  void DeviceBondedChanged(device::BluetoothAdapter* adapter,
                           device::BluetoothDevice* device,
                           bool new_bonded_status) override;
#endif
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  void DevicePairedChanged(device::BluetoothAdapter* adapter,
                           device::BluetoothDevice* device,
                           bool new_paired_status) override;
  void DeviceMTUChanged(device::BluetoothAdapter* adapter,
                        device::BluetoothDevice* device,
                        uint16_t mtu) override;
  void DeviceAdvertisementReceived(device::BluetoothAdapter* adapter,
                                   device::BluetoothDevice* device,
                                   int16_t rssi,
                                   const std::vector<uint8_t>& eir) override;
  void DeviceConnectedStateChanged(device::BluetoothAdapter* adapter,
                                   device::BluetoothDevice* device,
                                   bool is_now_connected) override;
#endif
  void DeviceRemoved(BluetoothAdapter* adapter,
                     BluetoothDevice* device) override;
  void GattServiceAdded(BluetoothAdapter* adapter,
                        BluetoothDevice* device,
                        BluetoothRemoteGattService* service) override;
  void GattServiceRemoved(BluetoothAdapter* adapter,
                          BluetoothDevice* device,
                          BluetoothRemoteGattService* service) override;
  void GattServicesDiscovered(BluetoothAdapter* adapter,
                              BluetoothDevice* device) override;
  void GattDiscoveryCompleteForService(
      BluetoothAdapter* adapter,
      BluetoothRemoteGattService* service) override;
  void GattServiceChanged(BluetoothAdapter* adapter,
                          BluetoothRemoteGattService* service) override;
  void GattCharacteristicAdded(
      BluetoothAdapter* adapter,
      BluetoothRemoteGattCharacteristic* characteristic) override;
  void GattCharacteristicRemoved(
      BluetoothAdapter* adapter,
      BluetoothRemoteGattCharacteristic* characteristic) override;
  void GattDescriptorAdded(BluetoothAdapter* adapter,
                           BluetoothRemoteGattDescriptor* descriptor) override;
  void GattDescriptorRemoved(
      BluetoothAdapter* adapter,
      BluetoothRemoteGattDescriptor* descriptor) override;
  void GattCharacteristicValueChanged(
      BluetoothAdapter* adapter,
      BluetoothRemoteGattCharacteristic* characteristic,
      const std::vector<uint8_t>& value) override;
  void GattDescriptorValueChanged(BluetoothAdapter* adapter,
                                  BluetoothRemoteGattDescriptor* descriptor,
                                  const std::vector<uint8_t>& value) override;
#if BUILDFLAG(IS_CHROMEOS)
  void LowEnergyScanSessionHardwareOffloadingStatusChanged(
      BluetoothAdapter::LowEnergyScanSessionHardwareOffloadingStatus status)
      override;
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Adapter related:
  int present_changed_count() const { return present_changed_count_; }
  int powered_changed_count() const { return powered_changed_count_; }
  int discoverable_changed_count() const { return discoverable_changed_count_; }
  int discovering_changed_count() const { return discovering_changed_count_; }
  bool last_present() const { return last_present_; }
  bool last_powered() const { return last_powered_; }
  bool last_discovering() const { return last_discovering_; }

  // Device related:
  int device_added_count() const { return device_added_count_; }
  int device_changed_count() const { return device_changed_count_; }
  int device_address_changed_count() const {
    return device_address_changed_count_;
  }

  // Advertisement related:
  int device_advertisement_raw_received_count() const {
    return device_advertisement_raw_received_count_;
  }
  std::string last_device_address() const { return last_device_address_; }
  const std::optional<std::string>& last_device_name() const {
    return last_device_name_;
  }
  const std::optional<std::string>& last_advertisement_name() const {
    return last_advertisement_name_;
  }
  const std::optional<int8_t>& last_rssi() const { return last_rssi_; }
  const std::optional<int8_t>& last_tx_power() const { return last_tx_power_; }
  const std::optional<uint16_t>& last_appearance() const {
    return last_appearance_;
  }
  const device::BluetoothDevice::UUIDList& last_advertised_uuids() const {
    return last_advertised_uuids_;
  }
  const device::BluetoothDevice::ServiceDataMap& last_service_data_map() const {
    return last_service_data_map_;
  }
  const device::BluetoothDevice::ManufacturerDataMap&
  last_manufacturer_data_map() const {
    return last_manufacturer_data_map_;
  }

#if BUILDFLAG(IS_CHROMEOS)
  int device_bonded_changed_count() const {
    return device_bonded_changed_count_;
  }
  bool device_new_bonded_status() const { return device_new_bonded_status_; }
#endif

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  int device_paired_changed_count() const {
    return device_paired_changed_count_;
  }
  bool device_new_paired_status() const { return device_new_paired_status_; }
  int device_mtu_changed_count() const { return device_mtu_changed_count_; }
  uint16_t last_mtu_value() const { return device_mtu_; }
  int device_advertisement_received_count() const {
    return device_advertisement_received_count_;
  }
  const std::vector<uint8_t>& device_eir() const { return device_eir_; }
  const std::vector<bool>& device_connected_state_changed_values() const {
    return device_connected_state_changed_values_;
  }
#endif
  int device_removed_count() const { return device_removed_count_; }
  BluetoothDevice* last_device() const { return last_device_; }

  // GATT related:
  int gatt_service_added_count() const { return gatt_service_added_count_; }
  int gatt_service_removed_count() const { return gatt_service_removed_count_; }
  int gatt_services_discovered_count() const {
    return gatt_services_discovered_count_;
  }
  int gatt_service_changed_count() const { return gatt_service_changed_count_; }
  int gatt_discovery_complete_count() const {
    return gatt_discovery_complete_count_;
  }
  int gatt_characteristic_added_count() const {
    return gatt_characteristic_added_count_;
  }
  int gatt_characteristic_removed_count() const {
    return gatt_characteristic_removed_count_;
  }
  int gatt_characteristic_value_changed_count() const {
    return gatt_characteristic_value_changed_count_;
  }
  int gatt_descriptor_added_count() const {
    return gatt_descriptor_added_count_;
  }
  int gatt_descriptor_removed_count() const {
    return gatt_descriptor_removed_count_;
  }
  int gatt_descriptor_value_changed_count() const {
    return gatt_descriptor_value_changed_count_;
  }
  std::string last_gatt_service_id() const { return last_gatt_service_id_; }
  BluetoothUUID last_gatt_service_uuid() const {
    return last_gatt_service_uuid_;
  }
  std::string last_gatt_characteristic_id() const {
    return last_gatt_characteristic_id_;
  }
  BluetoothUUID last_gatt_characteristic_uuid() const {
    return last_gatt_characteristic_uuid_;
  }
  std::vector<uint8_t> last_changed_characteristic_value() const {
    return last_changed_characteristic_value_;
  }
  std::vector<std::vector<uint8_t>>
  previous_characteristic_value_changed_values() const {
    return previous_characteristic_value_changed_values_;
  }
  std::string last_gatt_descriptor_id() const {
    return last_gatt_descriptor_id_;
  }
  BluetoothUUID last_gatt_descriptor_uuid() const {
    return last_gatt_descriptor_uuid_;
  }
  std::vector<uint8_t> last_changed_descriptor_value() const {
    return last_changed_descriptor_value_;
  }
#if BUILDFLAG(IS_CHROMEOS)
  BluetoothAdapter::LowEnergyScanSessionHardwareOffloadingStatus
  last_low_energy_scan_session_hardware_offloading_status() const {
    return last_low_energy_scan_session_hardware_offloading_status_;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

 private:
  // Some tests use a message loop since background processing is simulated;
  // break out of those loops.
  void QuitMessageLoop();

  scoped_refptr<BluetoothAdapter> adapter_;

  // Adapter related:
  int present_changed_count_;
  int powered_changed_count_;
  int discoverable_changed_count_;
  int discovering_changed_count_;
  bool last_present_;
  bool last_powered_;
  bool last_discovering_;

  // Device related:
  int device_added_count_;
  int device_changed_count_;
  int device_address_changed_count_;

  // Advertisement related
  int device_advertisement_raw_received_count_;
  std::string last_device_address_;
  std::optional<std::string> last_device_name_;
  std::optional<std::string> last_advertisement_name_;
  std::optional<int8_t> last_rssi_;
  std::optional<int8_t> last_tx_power_;
  std::optional<uint16_t> last_appearance_;
  device::BluetoothDevice::UUIDList last_advertised_uuids_;
  device::BluetoothDevice::ServiceDataMap last_service_data_map_;
  device::BluetoothDevice::ManufacturerDataMap last_manufacturer_data_map_;

  base::RepeatingClosure discovering_changed_callback_;
  base::RepeatingClosure discovery_change_completed_callback_;

  // RunLoop QuitClosure
  base::OnceClosure quit_closure_;

#if BUILDFLAG(IS_CHROMEOS)
  int device_bonded_changed_count_;
  bool device_new_bonded_status_;
#endif

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  int device_paired_changed_count_;
  bool device_new_paired_status_;
  int device_mtu_changed_count_;
  uint16_t device_mtu_;
  int device_advertisement_received_count_;
  std::vector<uint8_t> device_eir_;
  std::vector<bool> device_connected_state_changed_values_;
#endif
  int device_removed_count_;
  raw_ptr<BluetoothDevice> last_device_;

  // GATT related:
  int gatt_service_added_count_;
  int gatt_service_removed_count_;
  int gatt_services_discovered_count_;
  int gatt_service_changed_count_;
  int gatt_discovery_complete_count_;
  int gatt_characteristic_added_count_;
  int gatt_characteristic_removed_count_;
  int gatt_characteristic_value_changed_count_;
  int gatt_descriptor_added_count_;
  int gatt_descriptor_removed_count_;
  int gatt_descriptor_value_changed_count_;
  std::string last_gatt_service_id_;
  BluetoothUUID last_gatt_service_uuid_;
  std::string last_gatt_characteristic_id_;
  BluetoothUUID last_gatt_characteristic_uuid_;
  std::vector<uint8_t> last_changed_characteristic_value_;
  std::vector<std::vector<uint8_t>>
      previous_characteristic_value_changed_values_;
  std::string last_gatt_descriptor_id_;
  BluetoothUUID last_gatt_descriptor_uuid_;
  std::vector<uint8_t> last_changed_descriptor_value_;

#if BUILDFLAG(IS_CHROMEOS)
  BluetoothAdapter::LowEnergyScanSessionHardwareOffloadingStatus
      last_low_energy_scan_session_hardware_offloading_status_ =
          BluetoothAdapter::LowEnergyScanSessionHardwareOffloadingStatus::
              kUndetermined;
#endif  // BUILDFLAG(IS_CHROMEOS)
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_TEST_BLUETOOTH_ADAPTER_OBSERVER_H_

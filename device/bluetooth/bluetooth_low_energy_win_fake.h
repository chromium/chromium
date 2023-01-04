// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_WIN_FAKE_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_WIN_FAKE_H_

#include <memory>
#include <set>
#include <unordered_map>

#include "base/memory/raw_ptr.h"
#include "device/bluetooth/bluetooth_low_energy_win.h"

namespace device {
namespace win {

struct BLEDevice;
struct GattService;
struct GattCharacteristic;
struct GattCharacteristicObserver;
struct GattDescriptor;

// The key of BLEDevicesMap is the string of the BLE device address.
typedef std::unordered_map<std::string, std::unique_ptr<BLEDevice>>
    BLEDevicesMap;
// The key of GattServicesMap, GattCharacteristicsMap and GattDescriptorsMap is
// the string of the attribute handle.
typedef std::unordered_map<std::string, std::unique_ptr<GattService>>
    GattServicesMap;
typedef std::unordered_map<std::string, std::unique_ptr<GattCharacteristic>>
    GattCharacteristicsMap;
typedef std::unordered_map<std::string, std::unique_ptr<GattDescriptor>>
    GattDescriptorsMap;
// The key of BLEAttributeHandleTable is the string of the BLE device address.
typedef std::unordered_map<std::string, std::unique_ptr<std::set<USHORT>>>
    BLEAttributeHandleTable;
// The key of GattCharacteristicObserverTable is GattCharacteristicObserver
// pointer.
// Note: The underlying data type of BLUETOOTH_GATT_EVENT_HANDLE is PVOID.
typedef std::unordered_map<BLUETOOTH_GATT_EVENT_HANDLE,
                           std::unique_ptr<GattCharacteristicObserver>>
    GattCharacteristicObserverTable;

struct BLEDevice {
  BLEDevice();
  ~BLEDevice();
  std::unique_ptr<BluetoothLowEnergyDeviceInfo> device_info;
  GattServicesMap primary_services;
  bool marked_as_deleted;
};

struct GattService {
  GattService();
  ~GattService();
  std::unique_ptr<BTH_LE_GATT_SERVICE> service_info;
  GattServicesMap included_services;
  GattCharacteristicsMap included_characteristics;
};

struct GattCharacteristic {
  GattCharacteristic();
  ~GattCharacteristic();
  std::unique_ptr<BTH_LE_GATT_CHARACTERISTIC> characteristic_info;
  std::unique_ptr<BTH_LE_GATT_CHARACTERISTIC_VALUE> value;
  GattDescriptorsMap included_descriptors;
  std::vector<HRESULT> read_errors;
  std::vector<HRESULT> write_errors;
  std::vector<HRESULT> notify_errors;
  std::vector<BLUETOOTH_GATT_EVENT_HANDLE> observers;
};

struct GattDescriptor {
  GattDescriptor();
  ~GattDescriptor();
  std::unique_ptr<BTH_LE_GATT_DESCRIPTOR> descriptor_info;
  std::unique_ptr<BTH_LE_GATT_DESCRIPTOR_VALUE> value;
};

struct GattCharacteristicObserver {
  GattCharacteristicObserver();
  ~GattCharacteristicObserver();
  PFNBLUETOOTH_GATT_EVENT_CALLBACK_CORRECTED callback;
  PVOID context;
};

// Fake implementation of BluetoothLowEnergyWrapper. Used for BluetoothTestWin.
class BluetoothLowEnergyWrapperFake : public BluetoothLowEnergyWrapper {
 public:
  class Observer {
   public:
    Observer() {}
    ~Observer() {}

    virtual void OnReadGattCharacteristicValue() = 0;
    virtual void OnWriteGattCharacteristicValue(
        const PBTH_LE_GATT_CHARACTERISTIC_VALUE value) = 0;
    virtual void OnStartCharacteristicNotification() = 0;
    virtual void OnWriteGattDescriptorValue(
        const std::vector<uint8_t>& value) = 0;
  };

  BluetoothLowEnergyWrapperFake();
  ~BluetoothLowEnergyWrapperFake() override;

  bool EnumerateKnownBluetoothLowEnergyDevices(
      std::vector<std::unique_ptr<BluetoothLowEnergyDeviceInfo>>* devices,
      std::string* error) override;
  bool EnumerateKnownBluetoothLowEnergyGattServiceDevices(
      std::vector<std::unique_ptr<BluetoothLowEnergyDeviceInfo>>* devices,
      std::string* error) override;
  bool EnumerateKnownBluetoothLowEnergyServices(
      const base::FilePath& device_path,
      std::vector<std::unique_ptr<BluetoothLowEnergyServiceInfo>>* services,
      std::string* error) override;
  HRESULT ReadCharacteristicsOfAService(
      base::FilePath& service_path,
      const PBTH_LE_GATT_SERVICE service,
      std::unique_ptr<BTH_LE_GATT_CHARACTERISTIC>* out_included_characteristics,
      USHORT* out_counts) override;
  HRESULT ReadDescriptorsOfACharacteristic(
      base::FilePath& service_path,
      const PBTH_LE_GATT_CHARACTERISTIC characteristic,
      std::unique_ptr<BTH_LE_GATT_DESCRIPTOR>* out_included_descriptors,
      USHORT* out_counts) override;
  HRESULT ReadCharacteristicValue(
      base::FilePath& service_path,
      const PBTH_LE_GATT_CHARACTERISTIC characteristic,
      std::unique_ptr<BTH_LE_GATT_CHARACTERISTIC_VALUE>* out_value) override;
  HRESULT WriteCharacteristicValue(
      base::FilePath& service_path,
      const PBTH_LE_GATT_CHARACTERISTIC characteristic,
      PBTH_LE_GATT_CHARACTERISTIC_VALUE new_value,
      ULONG flags) override;
  HRESULT RegisterGattEvents(
      base::FilePath& service_path,
      BTH_LE_GATT_EVENT_TYPE type,
      PVOID event_parameter,
      PFNBLUETOOTH_GATT_EVENT_CALLBACK_CORRECTED callback,
      PVOID context,
      BLUETOOTH_GATT_EVENT_HANDLE* out_handle) override;
  HRESULT UnregisterGattEvent(
      BLUETOOTH_GATT_EVENT_HANDLE event_handle) override;
  HRESULT WriteDescriptorValue(
      base::FilePath& service_path,
      const PBTH_LE_GATT_DESCRIPTOR descriptor,
      PBTH_LE_GATT_DESCRIPTOR_VALUE new_value) override;

  BLEDevice* SimulateBLEDevice(std::string device_name,
                               BLUETOOTH_ADDRESS device_address);
  BLEDevice* GetSimulatedBLEDevice(std::string device_address);
  void RemoveSimulatedBLEDevice(std::string device_address);

  // Note: |parent_service| may be nullptr to indicate a primary service.
  GattService* SimulateGattService(BLEDevice* device,
                                   GattService* parent_service,
                                   const BTH_LE_UUID& uuid);

  // Note: |parent_service| may be nullptr to indicate a primary service.
  void SimulateGattServiceRemoved(BLEDevice* device,
                                  GattService* parent_service,
                                  std::string attribute_handle);

  // Note: |chain_of_att_handle| contains the attribute handles of the services
  // in order from primary service to target service. The last item in
  // |chain_of_att_handle| is the target service's attribute handle.
  GattService* GetSimulatedGattService(
      BLEDevice* device,
      const std::vector<std::string>& chain_of_att_handle);
  GattCharacteristic* SimulateGattCharacterisc(
      std::string device_address,
      GattService* parent_service,
      const BTH_LE_GATT_CHARACTERISTIC& characteristic);
  void SimulateGattCharacteriscRemove(GattService* parent_service,
                                      std::string attribute_handle);
  GattCharacteristic* GetSimulatedGattCharacteristic(
      GattService* parent_service,
      std::string attribute_handle);
  void SimulateGattCharacteristicValue(GattCharacteristic* characteristic,
                                       const std::vector<uint8_t>& value);
  void SimulateCharacteristicValueChangeNotification(
      GattCharacteristic* characteristic);
  void SimulateGattCharacteristicSetNotifyError(
      GattCharacteristic* characteristic,
      HRESULT error);
  void SimulateGattCharacteristicReadError(GattCharacteristic* characteristic,
                                           HRESULT error);
  void SimulateGattCharacteristicWriteError(GattCharacteristic* characteristic,
                                            HRESULT error);
  void RememberCharacteristicForSubsequentAction(GattService* parent_service,
                                                 std::string attribute_handle);
  void SimulateGattDescriptor(std::string device_address,
                              GattCharacteristic* characteristic,
                              const BTH_LE_UUID& uuid);
  void AddObserver(Observer* observer);

 private:
  // Get simulated characteristic by |service_path| and |characteristic| info.
  GattCharacteristic* GetSimulatedGattCharacteristic(
      base::FilePath& service_path,
      const PBTH_LE_GATT_CHARACTERISTIC characteristic);

  // Generate an unique attribute handle on |device_address|.
  USHORT GenerateAUniqueAttributeHandle(std::string device_address);

  // Generate device path for the BLE device with |device_address|.
  std::wstring GenerateBLEDevicePath(std::string device_address);

  // Generate GATT service device path of the service with
  // |service_attribute_handle|. |resident_device_path| is the BLE device this
  // GATT service belongs to.
  std::wstring GenerateGattServiceDevicePath(std::wstring resident_device_path,
                                             USHORT service_attribute_handle);

  // Extract device address from the device |path| generated by
  // GenerateBLEDevicePath or GenerateGattServiceDevicePath.
  std::wstring ExtractDeviceAddressFromDevicePath(std::wstring path);

  // Extract service attribute handles from the |path| generated by
  // GenerateGattServiceDevicePath.
  std::vector<std::string> ExtractServiceAttributeHandlesFromDevicePath(
      std::wstring path);

  // The canonical BLE device address string format is the
  // BluetoothDevice::CanonicalizeAddress.
  std::string BluetoothAddressToCanonicalString(const BLUETOOTH_ADDRESS& btha);

  // Table to store allocated attribute handle for a device.
  BLEAttributeHandleTable attribute_handle_table_;
  BLEDevicesMap simulated_devices_;
  raw_ptr<Observer> observer_;
  GattCharacteristicObserverTable gatt_characteristic_observers_;
  raw_ptr<GattCharacteristic> remembered_characteristic_;
};

}  // namespace win
}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_WIN_FAKE_H_

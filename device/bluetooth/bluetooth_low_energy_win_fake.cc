// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_low_energy_win_fake.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "device/bluetooth/bluetooth_low_energy_defs_win.h"

namespace {
const char kPlatformNotSupported[] =
    "Bluetooth Low energy is only supported on Windows 8 and later.";
}  // namespace

namespace device {
namespace win {

BLEDevice::BLEDevice() {}
BLEDevice::~BLEDevice() {}

GattService::GattService() {}
GattService::~GattService() {}

GattCharacteristic::GattCharacteristic() {}
GattCharacteristic::~GattCharacteristic() {}

GattDescriptor::GattDescriptor() {}
GattDescriptor::~GattDescriptor() {}

GattCharacteristicObserver::GattCharacteristicObserver() {}
GattCharacteristicObserver::~GattCharacteristicObserver() {}

BluetoothLowEnergyWrapperFake::BluetoothLowEnergyWrapperFake()
    : observer_(nullptr) {}
BluetoothLowEnergyWrapperFake::~BluetoothLowEnergyWrapperFake() {}

bool BluetoothLowEnergyWrapperFake::IsBluetoothLowEnergySupported() {
  return true;
}

bool BluetoothLowEnergyWrapperFake::EnumerateKnownBluetoothLowEnergyDevices(
    std::vector<std::unique_ptr<BluetoothLowEnergyDeviceInfo>>* devices,
    std::string* error) {
  if (!IsBluetoothLowEnergySupported()) {
    *error = kPlatformNotSupported;
    return false;
  }

  for (auto& device : simulated_devices_) {
    if (device.second->marked_as_deleted)
      continue;
    auto device_info = std::make_unique<BluetoothLowEnergyDeviceInfo>();
    *device_info = *(device.second->device_info);
    devices->push_back(std::move(device_info));
  }
  return true;
}

bool BluetoothLowEnergyWrapperFake::
    EnumerateKnownBluetoothLowEnergyGattServiceDevices(
        std::vector<std::unique_ptr<BluetoothLowEnergyDeviceInfo>>* devices,
        std::string* error) {
  if (!IsBluetoothLowEnergySupported()) {
    *error = kPlatformNotSupported;
    return false;
  }

  for (auto& device : simulated_devices_) {
    for (auto& service : device.second->primary_services) {
      auto device_info = std::make_unique<BluetoothLowEnergyDeviceInfo>();
      *device_info = *(device.second->device_info);
      base::string16 path = GenerateGattServiceDevicePath(
          device.second->device_info->path.value(),
          service.second->service_info->AttributeHandle);
      device_info->path = base::FilePath(path);
      devices->push_back(std::move(device_info));
    }
  }
  return true;
}

bool BluetoothLowEnergyWrapperFake::EnumerateKnownBluetoothLowEnergyServices(
    const base::FilePath& device_path,
    std::vector<std::unique_ptr<BluetoothLowEnergyServiceInfo>>* services,
    std::string* error) {
  if (!IsBluetoothLowEnergySupported()) {
    *error = kPlatformNotSupported;
    return false;
  }

  base::string16 device_address =
      ExtractDeviceAddressFromDevicePath(device_path.value());
  std::vector<std::string> service_attribute_handles =
      ExtractServiceAttributeHandlesFromDevicePath(device_path.value());

  BLEDevicesMap::iterator it_d = simulated_devices_.find(
      std::string(device_address.begin(), device_address.end()));
  CHECK(it_d != simulated_devices_.end());

  // |service_attribute_handles| is empty means |device_path| is a BLE device
  // path, otherwise it is a BLE GATT service device path.
  if (service_attribute_handles.empty()) {
    // Return all primary services for BLE device.
    for (auto& primary_service : it_d->second->primary_services) {
      auto service_info = std::make_unique<BluetoothLowEnergyServiceInfo>();
      service_info->uuid = primary_service.second->service_info->ServiceUuid;
      service_info->attribute_handle =
          primary_service.second->service_info->AttributeHandle;
      services->push_back(std::move(service_info));
    }
  } else {
    // Return corresponding GATT service for BLE GATT service device.
    GattService* target_service =
        GetSimulatedGattService(it_d->second.get(), service_attribute_handles);
    auto service_info = std::make_unique<BluetoothLowEnergyServiceInfo>();
    service_info->uuid = target_service->service_info->ServiceUuid;
    service_info->attribute_handle =
        target_service->service_info->AttributeHandle;
    services->push_back(std::move(service_info));
  }

  return true;
}

HRESULT BluetoothLowEnergyWrapperFake::ReadCharacteristicsOfAService(
    base::FilePath& service_path,
    const PBTH_LE_GATT_SERVICE service,
    std::unique_ptr<BTH_LE_GATT_CHARACTERISTIC>* out_included_characteristics,
    USHORT* out_counts) {
  base::string16 device_address =
      ExtractDeviceAddressFromDevicePath(service_path.value());
  BLEDevice* target_device = GetSimulatedBLEDevice(
      std::string(device_address.begin(), device_address.end()));
  if (target_device == nullptr)
    return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
  const std::vector<std::string> service_att_handles =
      ExtractServiceAttributeHandlesFromDevicePath(service_path.value());
  GattService* target_service =
      GetSimulatedGattService(target_device, service_att_handles);
  if (target_service == nullptr)
    return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);

  std::size_t number_of_included_characteristic =
      target_service->included_characteristics.size();
  if (number_of_included_characteristic) {
    *out_counts = (USHORT)number_of_included_characteristic;
    out_included_characteristics->reset(
        new BTH_LE_GATT_CHARACTERISTIC[number_of_included_characteristic]);
    std::size_t i = 0;
    for (const auto& cha : target_service->included_characteristics) {
      out_included_characteristics->get()[i] =
          *(cha.second->characteristic_info);
      i++;
    }
    return S_OK;
  }
  return HRESULT_FROM_WIN32(ERROR_NO_MORE_ITEMS);
}

HRESULT BluetoothLowEnergyWrapperFake::ReadDescriptorsOfACharacteristic(
    base::FilePath& service_path,
    const PBTH_LE_GATT_CHARACTERISTIC characteristic,
    std::unique_ptr<BTH_LE_GATT_DESCRIPTOR>* out_included_descriptors,
    USHORT* out_counts) {
  GattCharacteristic* target_characteristic =
      GetSimulatedGattCharacteristic(service_path, characteristic);
  if (target_characteristic == nullptr)
    return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);

  std::size_t number_of_included_descriptors =
      target_characteristic->included_descriptors.size();
  PBTH_LE_GATT_DESCRIPTOR win_descriptors_info =
      new BTH_LE_GATT_DESCRIPTOR[number_of_included_descriptors];
  *out_counts = USHORT(number_of_included_descriptors);
  std::size_t i = 0;
  for (const auto& d : target_characteristic->included_descriptors) {
    win_descriptors_info[i] = *(d.second->descriptor_info);
    i++;
  }
  out_included_descriptors->reset(win_descriptors_info);
  return S_OK;
}

HRESULT BluetoothLowEnergyWrapperFake::ReadCharacteristicValue(
    base::FilePath& service_path,
    const PBTH_LE_GATT_CHARACTERISTIC characteristic,
    std::unique_ptr<BTH_LE_GATT_CHARACTERISTIC_VALUE>* out_value) {
  GattCharacteristic* target_characteristic =
      GetSimulatedGattCharacteristic(service_path, characteristic);
  if (target_characteristic == nullptr)
    return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);

  // Return error simulated by SimulateGattCharacteristicReadError.
  if (target_characteristic->read_errors.size()) {
    HRESULT hr = target_characteristic->read_errors[0];
    target_characteristic->read_errors.erase(
        target_characteristic->read_errors.begin());
    return hr;
  }

  PBTH_LE_GATT_CHARACTERISTIC_VALUE ret_value =
      (PBTH_LE_GATT_CHARACTERISTIC_VALUE)(
          new UCHAR[sizeof(ULONG) + target_characteristic->value->DataSize]);
  ret_value->DataSize = target_characteristic->value->DataSize;
  for (ULONG i = 0; i < ret_value->DataSize; i++)
    ret_value->Data[i] = target_characteristic->value->Data[i];
  out_value->reset(ret_value);
  if (observer_)
    observer_->OnReadGattCharacteristicValue();
  return S_OK;
}

HRESULT BluetoothLowEnergyWrapperFake::WriteCharacteristicValue(
    base::FilePath& service_path,
    const PBTH_LE_GATT_CHARACTERISTIC characteristic,
    PBTH_LE_GATT_CHARACTERISTIC_VALUE new_value) {
  GattCharacteristic* target_characteristic =
      GetSimulatedGattCharacteristic(service_path, characteristic);
  if (target_characteristic == nullptr)
    return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);

  // Return error simulated by SimulateGattCharacteristicWriteError.
  if (target_characteristic->write_errors.size()) {
    HRESULT hr = *(target_characteristic->write_errors.begin());
    target_characteristic->write_errors.erase(
        target_characteristic->write_errors.begin());
    return hr;
  }

  PBTH_LE_GATT_CHARACTERISTIC_VALUE win_value =
      (PBTH_LE_GATT_CHARACTERISTIC_VALUE)(
          new UCHAR[new_value->DataSize + sizeof(ULONG)]);
  for (ULONG i = 0; i < new_value->DataSize; i++)
    win_value->Data[i] = new_value->Data[i];
  win_value->DataSize = new_value->DataSize;
  target_characteristic->value.reset(win_value);
  if (observer_)
    observer_->OnWriteGattCharacteristicValue(win_value);
  return S_OK;
}

HRESULT BluetoothLowEnergyWrapperFake::RegisterGattEvents(
    base::FilePath& service_path,
    BTH_LE_GATT_EVENT_TYPE type,
    PVOID event_parameter,
    PFNBLUETOOTH_GATT_EVENT_CALLBACK_CORRECTED callback,
    PVOID context,
    BLUETOOTH_GATT_EVENT_HANDLE* out_handle) {
  // Right now, only CharacteristicValueChangedEvent is supported.
  CHECK(CharacteristicValueChangedEvent == type);

  std::unique_ptr<GattCharacteristicObserver> observer(
      new GattCharacteristicObserver());
  observer->callback = callback;
  observer->context = context;
  *out_handle = (BLUETOOTH_GATT_EVENT_HANDLE)observer.get();

  PBLUETOOTH_GATT_VALUE_CHANGED_EVENT_REGISTRATION parameter =
      (PBLUETOOTH_GATT_VALUE_CHANGED_EVENT_REGISTRATION)event_parameter;
  for (USHORT i = 0; i < parameter->NumCharacteristics; i++) {
    GattCharacteristic* target_characteristic = GetSimulatedGattCharacteristic(
        service_path, &parameter->Characteristics[i]);
    CHECK(target_characteristic);

    // Return error simulated by SimulateGattCharacteristicSetNotifyError.
    if (target_characteristic->notify_errors.size()) {
      HRESULT error = target_characteristic->notify_errors[0];
      target_characteristic->notify_errors.erase(
          target_characteristic->notify_errors.begin());
      return error;
    }

    target_characteristic->observers.push_back(*out_handle);
  }
  gatt_characteristic_observers_[*out_handle] = std::move(observer);

  if (observer_)
    observer_->OnStartCharacteristicNotification();

  return S_OK;
}

HRESULT BluetoothLowEnergyWrapperFake::UnregisterGattEvent(
    BLUETOOTH_GATT_EVENT_HANDLE event_handle) {
  gatt_characteristic_observers_.erase(event_handle);
  return S_OK;
}

HRESULT BluetoothLowEnergyWrapperFake::WriteDescriptorValue(
    base::FilePath& service_path,
    const PBTH_LE_GATT_DESCRIPTOR descriptor,
    PBTH_LE_GATT_DESCRIPTOR_VALUE new_value) {
  if (new_value->DescriptorType == ClientCharacteristicConfiguration) {
    // Simulate the value the OS will write.
    std::vector<UCHAR> write_value;
    if (new_value->ClientCharacteristicConfiguration
            .IsSubscribeToNotification) {
      write_value.push_back(1);
    } else if (new_value->ClientCharacteristicConfiguration
                   .IsSubscribeToIndication) {
      write_value.push_back(2);
    }
    write_value.push_back(0);
    if (observer_)
      observer_->OnWriteGattDescriptorValue(write_value);
  }
  return S_OK;
}

BLEDevice* BluetoothLowEnergyWrapperFake::SimulateBLEDevice(
    std::string device_name,
    BLUETOOTH_ADDRESS device_address) {
  BLEDevice* device = new BLEDevice();
  BluetoothLowEnergyDeviceInfo* device_info =
      new BluetoothLowEnergyDeviceInfo();
  std::string string_device_address =
      BluetoothAddressToCanonicalString(device_address);
  device_info->path =
      base::FilePath(GenerateBLEDevicePath(string_device_address));
  device_info->friendly_name = device_name;
  device_info->address = device_address;
  device->device_info.reset(device_info);
  device->marked_as_deleted = false;
  simulated_devices_[string_device_address] = base::WrapUnique(device);
  return device;
}

BLEDevice* BluetoothLowEnergyWrapperFake::GetSimulatedBLEDevice(
    std::string device_address) {
  BLEDevicesMap::iterator it_d = simulated_devices_.find(device_address);
  if (it_d == simulated_devices_.end())
    return nullptr;
  return it_d->second.get();
}

void BluetoothLowEnergyWrapperFake::RemoveSimulatedBLEDevice(
    std::string device_address) {
  simulated_devices_[device_address]->marked_as_deleted = true;
}

GattService* BluetoothLowEnergyWrapperFake::SimulateGattService(
    BLEDevice* device,
    GattService* parent_service,
    const BTH_LE_UUID& uuid) {
  CHECK(device);

  GattService* service = new GattService();
  PBTH_LE_GATT_SERVICE service_info = new BTH_LE_GATT_SERVICE[1];
  std::string string_device_address =
      BluetoothAddressToCanonicalString(device->device_info->address);
  service_info->AttributeHandle =
      GenerateAUniqueAttributeHandle(string_device_address);
  service_info->ServiceUuid = uuid;
  service->service_info.reset(service_info);

  if (parent_service) {
    parent_service
        ->included_services[std::to_string(service_info->AttributeHandle)] =
        base::WrapUnique(service);
  } else {
    device->primary_services[std::to_string(service_info->AttributeHandle)] =
        base::WrapUnique(service);
  }
  return service;
}

void BluetoothLowEnergyWrapperFake::SimulateGattServiceRemoved(
    BLEDevice* device,
    GattService* parent_service,
    std::string attribute_handle) {
  if (parent_service) {
    parent_service->included_services.erase(attribute_handle);
  } else {
    device->primary_services.erase(attribute_handle);
  }
}

GattService* BluetoothLowEnergyWrapperFake::GetSimulatedGattService(
    BLEDevice* device,
    const std::vector<std::string>& chain_of_att_handle) {
  // First, find the root primary service.
  GattServicesMap::iterator it_s =
      device->primary_services.find(chain_of_att_handle[0]);
  if (it_s == device->primary_services.end())
    return nullptr;

  // Iteratively follow the chain of included service attribute handles to find
  // the target service.
  for (std::size_t i = 1; i < chain_of_att_handle.size(); i++) {
    std::string included_att_handle = std::string(
        chain_of_att_handle[i].begin(), chain_of_att_handle[i].end());
    GattServicesMap::iterator it_i =
        it_s->second->included_services.find(included_att_handle);
    if (it_i == it_s->second->included_services.end())
      return nullptr;
    it_s = it_i;
  }
  return it_s->second.get();
}

GattCharacteristic* BluetoothLowEnergyWrapperFake::SimulateGattCharacterisc(
    std::string device_address,
    GattService* parent_service,
    const BTH_LE_GATT_CHARACTERISTIC& characteristic) {
  CHECK(parent_service);

  GattCharacteristic* win_characteristic = new GattCharacteristic();
  PBTH_LE_GATT_CHARACTERISTIC win_characteristic_info =
      new BTH_LE_GATT_CHARACTERISTIC[1];
  *win_characteristic_info = characteristic;
  (win_characteristic->characteristic_info).reset(win_characteristic_info);
  win_characteristic->characteristic_info->AttributeHandle =
      GenerateAUniqueAttributeHandle(device_address);
  parent_service->included_characteristics[std::to_string(
      win_characteristic->characteristic_info->AttributeHandle)] =
      base::WrapUnique(win_characteristic);
  // Set default empty value.
  PBTH_LE_GATT_CHARACTERISTIC_VALUE win_value =
      (PBTH_LE_GATT_CHARACTERISTIC_VALUE)(
          new UCHAR[sizeof(BTH_LE_GATT_CHARACTERISTIC_VALUE)]);
  win_value->DataSize = 0;
  win_characteristic->value.reset(win_value);
  return win_characteristic;
}

void BluetoothLowEnergyWrapperFake::SimulateGattCharacteriscRemove(
    GattService* parent_service,
    std::string attribute_handle) {
  CHECK(parent_service);
  parent_service->included_characteristics.erase(attribute_handle);
}

GattCharacteristic*
BluetoothLowEnergyWrapperFake::GetSimulatedGattCharacteristic(
    GattService* parent_service,
    std::string attribute_handle) {
  CHECK(parent_service);
  GattCharacteristicsMap::iterator it =
      parent_service->included_characteristics.find(attribute_handle);
  if (it != parent_service->included_characteristics.end())
    return it->second.get();
  return nullptr;
}

void BluetoothLowEnergyWrapperFake::SimulateGattCharacteristicValue(
    GattCharacteristic* characteristic,
    const std::vector<uint8_t>& value) {
  GattCharacteristic* target_characteristic = characteristic;
  if (target_characteristic == nullptr)
    target_characteristic = remembered_characteristic_;
  CHECK(target_characteristic);

  PBTH_LE_GATT_CHARACTERISTIC_VALUE win_value =
      (PBTH_LE_GATT_CHARACTERISTIC_VALUE)(
          new UCHAR[value.size() + sizeof(ULONG)]);
  win_value->DataSize = (ULONG)value.size();
  for (std::size_t i = 0; i < value.size(); i++)
    win_value->Data[i] = value[i];
  target_characteristic->value.reset(win_value);
}

void BluetoothLowEnergyWrapperFake::
    SimulateCharacteristicValueChangeNotification(
        GattCharacteristic* characteristic) {
  GattCharacteristic* target_characteristic = characteristic;
  if (target_characteristic == nullptr)
    target_characteristic = remembered_characteristic_;
  CHECK(target_characteristic);
  for (auto* observer : target_characteristic->observers) {
    GattCharacteristicObserverTable::const_iterator it =
        gatt_characteristic_observers_.find(observer);
    // Check if |observer| has been unregistered by UnregisterGattEvent.
    if (it != gatt_characteristic_observers_.end()) {
      BLUETOOTH_GATT_VALUE_CHANGED_EVENT event;
      event.ChangedAttributeHandle =
          target_characteristic->characteristic_info->AttributeHandle;
      event.CharacteristicValueDataSize =
          target_characteristic->value->DataSize + sizeof(ULONG);
      event.CharacteristicValue = target_characteristic->value.get();
      it->second->callback(CharacteristicValueChangedEvent, &event,
                           it->second->context);
    }
  }
}

void BluetoothLowEnergyWrapperFake::SimulateGattCharacteristicSetNotifyError(
    GattCharacteristic* characteristic,
    HRESULT error) {
  characteristic->notify_errors.push_back(error);
}

void BluetoothLowEnergyWrapperFake::SimulateGattCharacteristicReadError(
    GattCharacteristic* characteristic,
    HRESULT error) {
  CHECK(characteristic);
  characteristic->read_errors.push_back(error);
}

void BluetoothLowEnergyWrapperFake::SimulateGattCharacteristicWriteError(
    GattCharacteristic* characteristic,
    HRESULT error) {
  CHECK(characteristic);
  characteristic->write_errors.push_back(error);
}

void BluetoothLowEnergyWrapperFake::RememberCharacteristicForSubsequentAction(
    GattService* parent_service,
    std::string attribute_handle) {
  CHECK(parent_service);
  remembered_characteristic_ =
      parent_service->included_characteristics[attribute_handle].get();
  CHECK(remembered_characteristic_);
}

void BluetoothLowEnergyWrapperFake::SimulateGattDescriptor(
    std::string device_address,
    GattCharacteristic* characteristic,
    const BTH_LE_UUID& uuid) {
  std::unique_ptr<GattDescriptor> descriptor(new GattDescriptor());
  descriptor->descriptor_info.reset(new BTH_LE_GATT_DESCRIPTOR[1]);
  descriptor->descriptor_info->DescriptorUuid = uuid;
  descriptor->descriptor_info->AttributeHandle =
      GenerateAUniqueAttributeHandle(device_address);
  characteristic->included_descriptors[std::to_string(
      descriptor->descriptor_info->AttributeHandle)] = std::move(descriptor);
}

void BluetoothLowEnergyWrapperFake::AddObserver(Observer* observer) {
  observer_ = observer;
}

GattCharacteristic*
BluetoothLowEnergyWrapperFake::GetSimulatedGattCharacteristic(
    base::FilePath& service_path,
    const PBTH_LE_GATT_CHARACTERISTIC characteristic) {
  base::string16 device_address =
      ExtractDeviceAddressFromDevicePath(service_path.value());
  BLEDevice* target_device = GetSimulatedBLEDevice(
      std::string(device_address.begin(), device_address.end()));
  if (target_device == nullptr)
    return nullptr;
  const std::vector<std::string> service_att_handles =
      ExtractServiceAttributeHandlesFromDevicePath(service_path.value());
  GattService* target_service =
      GetSimulatedGattService(target_device, service_att_handles);
  if (target_service == nullptr)
    return nullptr;
  GattCharacteristic* target_characteristic = GetSimulatedGattCharacteristic(
      target_service, std::to_string(characteristic->AttributeHandle));
  return target_characteristic;
}

USHORT BluetoothLowEnergyWrapperFake::GenerateAUniqueAttributeHandle(
    std::string device_address) {
  std::unique_ptr<std::set<USHORT>>& set_of_ushort =
      attribute_handle_table_[device_address];
  if (set_of_ushort) {
    USHORT max_attribute_handle = *set_of_ushort->rbegin();
    if (max_attribute_handle < 0xFFFF) {
      USHORT new_attribute_handle = max_attribute_handle + 1;
      set_of_ushort->insert(new_attribute_handle);
      return new_attribute_handle;
    } else {
      USHORT i = 1;
      for (; i < 0xFFFF; i++) {
        if (set_of_ushort->find(i) == set_of_ushort->end())
          break;
      }
      if (i >= 0xFFFF)
        return 0;
      set_of_ushort->insert(i);
      return i;
    }
  }

  USHORT smallest_att_handle = 1;
  std::set<USHORT>* new_set = new std::set<USHORT>();
  new_set->insert(smallest_att_handle);
  set_of_ushort.reset(new_set);
  return smallest_att_handle;
}

base::string16 BluetoothLowEnergyWrapperFake::GenerateBLEDevicePath(
    std::string device_address) {
  return base::string16(device_address.begin(), device_address.end());
}

base::string16 BluetoothLowEnergyWrapperFake::GenerateGattServiceDevicePath(
    base::string16 resident_device_path,
    USHORT service_attribute_handle) {
  std::string sub_path = std::to_string(service_attribute_handle);
  return resident_device_path + STRING16_LITERAL("/") +
         base::string16(sub_path.begin(), sub_path.end());
}

base::string16
BluetoothLowEnergyWrapperFake::ExtractDeviceAddressFromDevicePath(
    base::string16 path) {
  std::size_t found = path.find_first_of('/');
  if (found != base::string16::npos) {
    return path.substr(0, found);
  }
  return path;
}

std::vector<std::string>
BluetoothLowEnergyWrapperFake::ExtractServiceAttributeHandlesFromDevicePath(
    base::string16 path) {
  std::size_t found = path.find('/');
  if (found == base::string16::npos)
    return std::vector<std::string>();

  std::vector<std::string> chain_of_att_handle;
  while (true) {
    std::size_t next_found = path.find(path, found + 1);
    if (next_found == base::string16::npos)
      break;
    base::string16 att_handle = path.substr(found + 1, next_found);
    chain_of_att_handle.push_back(
        std::string(att_handle.begin(), att_handle.end()));
    found = next_found;
  }
  base::string16 att_handle = path.substr(found + 1);
  chain_of_att_handle.push_back(
      std::string(att_handle.begin(), att_handle.end()));
  return chain_of_att_handle;
}

std::string BluetoothLowEnergyWrapperFake::BluetoothAddressToCanonicalString(
    const BLUETOOTH_ADDRESS& btha) {
  std::string result = base::StringPrintf(
      "%02X:%02X:%02X:%02X:%02X:%02X", btha.rgBytes[5], btha.rgBytes[4],
      btha.rgBytes[3], btha.rgBytes[2], btha.rgBytes[1], btha.rgBytes[0]);
  return result;
}

}  // namespace win
}  // namespace device

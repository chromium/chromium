// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_gatt_discoverer_winrt.h"

#include <windows.foundation.collections.h>

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/win/post_async_results.h"
#include "components/device_event_log/device_event_log.h"
#include "device/bluetooth/bluetooth_remote_gatt_service_winrt.h"

namespace device {

namespace {

using ABI::Windows::Devices::Bluetooth::IBluetoothLEDevice;
using ABI::Windows::Devices::Bluetooth::IBluetoothLEDevice3;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCharacteristic;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCharacteristicsResult;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCommunicationStatus;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCommunicationStatus_AccessDenied;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCommunicationStatus_Success;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::GattDescriptor;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattDescriptorsResult;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattDeviceService;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattDeviceServicesResult;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::GattOpenStatus;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattOpenStatus_AlreadyOpened;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattOpenStatus_Success;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattSharingMode;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattSharingMode_SharedReadAndWrite;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    IGattCharacteristic3;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    IGattCharacteristicsResult;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    IGattDescriptorsResult;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    IGattDeviceService;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    IGattDeviceService3;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    IGattDeviceServicesResult;
using ABI::Windows::Foundation::IAsyncOperation;
using ABI::Windows::Foundation::IReference;
using ABI::Windows::Foundation::Collections::IVectorView;
using Microsoft::WRL::ComPtr;

template <typename IGattResult>
bool CheckCommunicationStatus(IGattResult* gatt_result,
                              bool allow_access_denied = false) {
  if (!gatt_result) {
    BLUETOOTH_LOG(DEBUG) << "Getting GATT Results failed.";
    return false;
  }

  GattCommunicationStatus status;
  HRESULT hr = gatt_result->get_Status(&status);
  if (FAILED(hr)) {
    BLUETOOTH_LOG(DEBUG) << "Getting GATT Communication Status failed: "
                         << logging::SystemErrorCodeToString(hr);
    return false;
  }

  if (status != GattCommunicationStatus_Success) {
    if (status == GattCommunicationStatus_AccessDenied) {
      BLUETOOTH_LOG(DEBUG) << "GATT access denied error";
    } else {
      BLUETOOTH_LOG(DEBUG) << "Unexpected GattCommunicationStatus: " << status;
    }
    BLUETOOTH_LOG(DEBUG)
        << "GATT Error Code: "
        << static_cast<int>(
               BluetoothRemoteGattServiceWinrt::GetGattErrorCode(gatt_result));
  }

  return status == GattCommunicationStatus_Success ||
         (allow_access_denied &&
          status == GattCommunicationStatus_AccessDenied);
}

template <typename T, typename I>
bool GetAsVector(IVectorView<T*>* view, std::vector<ComPtr<I>>* vector) {
  unsigned size;
  HRESULT hr = view->get_Size(&size);
  if (FAILED(hr)) {
    BLUETOOTH_LOG(DEBUG) << "Getting Size failed: "
                         << logging::SystemErrorCodeToString(hr);
    return false;
  }

  vector->reserve(size);
  for (unsigned i = 0; i < size; ++i) {
    ComPtr<I> entry;
    hr = view->GetAt(i, &entry);
    if (FAILED(hr)) {
      BLUETOOTH_LOG(DEBUG) << "GetAt(" << i << ") failed: "
                           << logging::SystemErrorCodeToString(hr);
      return false;
    }

    vector->push_back(std::move(entry));
  }

  return true;
}

}  // namespace

BluetoothGattDiscovererWinrt::BluetoothGattDiscovererWinrt(
    ComPtr<IBluetoothLEDevice> ble_device)
    : ble_device_(std::move(ble_device)) {}

BluetoothGattDiscovererWinrt::~BluetoothGattDiscovererWinrt() = default;

void BluetoothGattDiscovererWinrt::StartGattDiscovery(
    GattDiscoveryCallback callback) {
  callback_ = std::move(callback);
  ComPtr<IBluetoothLEDevice3> ble_device_3;
  HRESULT hr = ble_device_.As(&ble_device_3);
  if (FAILED(hr)) {
    BLUETOOTH_LOG(DEBUG) << "Obtaining IBluetoothLEDevice3 failed: "
                         << logging::SystemErrorCodeToString(hr);
    std::move(callback_).Run(false);
    return;
  }

  ComPtr<IAsyncOperation<GattDeviceServicesResult*>> get_gatt_services_op;
  hr = ble_device_3->GetGattServicesAsync(&get_gatt_services_op);
  if (FAILED(hr)) {
    BLUETOOTH_LOG(DEBUG) << "BluetoothLEDevice::GetGattServicesAsync failed: "
                         << logging::SystemErrorCodeToString(hr);
    std::move(callback_).Run(false);
    return;
  }

  hr = base::win::PostAsyncResults(
      std::move(get_gatt_services_op),
      base::BindOnce(&BluetoothGattDiscovererWinrt::OnGetGattServices,
                     weak_ptr_factory_.GetWeakPtr()));

  if (FAILED(hr)) {
    BLUETOOTH_LOG(DEBUG) << "PostAsyncResults failed: "
                         << logging::SystemErrorCodeToString(hr);
    std::move(callback_).Run(false);
  }
}

const BluetoothGattDiscovererWinrt::GattServiceList&
BluetoothGattDiscovererWinrt::GetGattServices() const {
  return gatt_services_;
}

const BluetoothGattDiscovererWinrt::GattCharacteristicList*
BluetoothGattDiscovererWinrt::GetCharacteristics(
    uint16_t service_attribute_handle) const {
  auto iter = service_to_characteristics_map_.find(service_attribute_handle);
  return iter != service_to_characteristics_map_.end() ? &iter->second
                                                       : nullptr;
}

const BluetoothGattDiscovererWinrt::GattDescriptorList*
BluetoothGattDiscovererWinrt::GetDescriptors(
    uint16_t characteristic_attribute_handle) const {
  auto iter =
      characteristic_to_descriptors_map_.find(characteristic_attribute_handle);
  return iter != characteristic_to_descriptors_map_.end() ? &iter->second
                                                          : nullptr;
}

void BluetoothGattDiscovererWinrt::OnGetGattServices(
    ComPtr<IGattDeviceServicesResult> services_result) {
  if (!CheckCommunicationStatus(services_result.Get())) {
    BLUETOOTH_LOG(DEBUG) << "Failed to get GATT services.";
    std::move(callback_).Run(false);
    return;
  }

  ComPtr<IVectorView<GattDeviceService*>> services;
  HRESULT hr = services_result->get_Services(&services);
  if (FAILED(hr)) {
    BLUETOOTH_LOG(DEBUG) << "Getting GATT Services failed: "
                         << logging::SystemErrorCodeToString(hr);
    std::move(callback_).Run(false);
    return;
  }

  if (!GetAsVector(services.Get(), &gatt_services_)) {
    std::move(callback_).Run(false);
    return;
  }

  num_services_ = gatt_services_.size();
  for (const auto& gatt_service : gatt_services_) {
    uint16_t service_attribute_handle;
    hr = gatt_service->get_AttributeHandle(&service_attribute_handle);
    if (FAILED(hr)) {
      BLUETOOTH_LOG(DEBUG) << "Getting AttributeHandle failed: "
                           << logging::SystemErrorCodeToString(hr);
      std::move(callback_).Run(false);
      return;
    }

    ComPtr<IGattDeviceService3> gatt_service_3;
    hr = gatt_service.As(&gatt_service_3);
    if (FAILED(hr)) {
      BLUETOOTH_LOG(DEBUG) << "Obtaining IGattDeviceService3 failed: "
                           << logging::SystemErrorCodeToString(hr);
      std::move(callback_).Run(false);
      return;
    }

    ComPtr<IAsyncOperation<GattOpenStatus>> open_op;
    hr =
        gatt_service_3->OpenAsync(GattSharingMode_SharedReadAndWrite, &open_op);
    if (FAILED(hr)) {
      BLUETOOTH_LOG(DEBUG) << "GattDeviceService::OpenAsync() failed: "
                           << logging::SystemErrorCodeToString(hr);
      std::move(callback_).Run(false);
    }

    hr = base::win::PostAsyncResults(
        std::move(open_op),
        base::BindOnce(&BluetoothGattDiscovererWinrt::OnServiceOpen,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(gatt_service_3), service_attribute_handle));
  }

  RunCallbackIfDone();
}

void BluetoothGattDiscovererWinrt::OnServiceOpen(
    ComPtr<IGattDeviceService3> gatt_service_3,
    uint16_t service_attribute_handle,
    GattOpenStatus status) {
  if (status != GattOpenStatus_Success &&
      status != GattOpenStatus_AlreadyOpened) {
    BLUETOOTH_LOG(DEBUG) << "Failed to open service "
                         << service_attribute_handle << ": " << status;
    std::move(callback_).Run(false);
    return;
  }


  ComPtr<IAsyncOperation<GattCharacteristicsResult*>> get_characteristics_op;
  HRESULT hr = gatt_service_3->GetCharacteristicsAsync(&get_characteristics_op);
  if (FAILED(hr)) {
    BLUETOOTH_LOG(DEBUG)
        << "GattDeviceService::GetCharacteristicsAsync() failed: "
        << logging::SystemErrorCodeToString(hr);
    std::move(callback_).Run(false);
    return;
  }

  hr = base::win::PostAsyncResults(
      std::move(get_characteristics_op),
      base::BindOnce(&BluetoothGattDiscovererWinrt::OnGetCharacteristics,
                     weak_ptr_factory_.GetWeakPtr(), service_attribute_handle));

  if (FAILED(hr)) {
    BLUETOOTH_LOG(DEBUG) << "PostAsyncResults failed: "
                         << logging::SystemErrorCodeToString(hr);
    std::move(callback_).Run(false);
  }
}

void BluetoothGattDiscovererWinrt::OnGetCharacteristics(
    uint16_t service_attribute_handle,
    ComPtr<IGattCharacteristicsResult> characteristics_result) {
  // A few GATT services like HID over GATT (short UUID 0x1812) are protected
  // by the OS, leading to an access denied error.
  if (!CheckCommunicationStatus(characteristics_result.Get(), true)) {
    BLUETOOTH_LOG(DEBUG) << "Failed to get characteristics for service "
                         << service_attribute_handle << ".";
    std::move(callback_).Run(false);
    return;
  }

  ComPtr<IVectorView<GattCharacteristic*>> characteristics;
  HRESULT hr = characteristics_result->get_Characteristics(&characteristics);
  if (FAILED(hr)) {
    BLUETOOTH_LOG(DEBUG) << "Getting Characteristics failed: "
                         << logging::SystemErrorCodeToString(hr);
    std::move(callback_).Run(false);
    return;
  }

  DCHECK(!base::Contains(service_to_characteristics_map_,
                         service_attribute_handle));
  auto& characteristics_list =
      service_to_characteristics_map_[service_attribute_handle];
  if (!GetAsVector(characteristics.Get(), &characteristics_list)) {
    std::move(callback_).Run(false);
    return;
  }

  num_characteristics_ += characteristics_list.size();
  for (const auto& gatt_characteristic : characteristics_list) {
    uint16_t characteristic_attribute_handle;
    hr = gatt_characteristic->get_AttributeHandle(
        &characteristic_attribute_handle);
    if (FAILED(hr)) {
      BLUETOOTH_LOG(DEBUG) << "Getting AttributeHandle failed: "
                           << logging::SystemErrorCodeToString(hr);
      std::move(callback_).Run(false);
      return;
    }

    ComPtr<IGattCharacteristic3> gatt_characteristic_3;
    hr = gatt_characteristic.As(&gatt_characteristic_3);
    if (FAILED(hr)) {
      BLUETOOTH_LOG(DEBUG) << "Obtaining IGattCharacteristic3 failed: "
                           << logging::SystemErrorCodeToString(hr);
      std::move(callback_).Run(false);
      return;
    }

    ComPtr<IAsyncOperation<GattDescriptorsResult*>> get_descriptors_op;
    hr = gatt_characteristic_3->GetDescriptorsAsync(&get_descriptors_op);
    if (FAILED(hr)) {
      BLUETOOTH_LOG(DEBUG)
          << "GattCharacteristic::GetDescriptorsAsync() failed: "
          << logging::SystemErrorCodeToString(hr);
      std::move(callback_).Run(false);
      return;
    }

    hr = base::win::PostAsyncResults(
        std::move(get_descriptors_op),
        base::BindOnce(&BluetoothGattDiscovererWinrt::OnGetDescriptors,
                       weak_ptr_factory_.GetWeakPtr(),
                       characteristic_attribute_handle));

    if (FAILED(hr)) {
      BLUETOOTH_LOG(DEBUG) << "PostAsyncResults failed: "
                           << logging::SystemErrorCodeToString(hr);
      std::move(callback_).Run(false);
    }
  }

  RunCallbackIfDone();
}

void BluetoothGattDiscovererWinrt::OnGetDescriptors(
    uint16_t characteristic_attribute_handle,
    ComPtr<IGattDescriptorsResult> descriptors_result) {
  if (!CheckCommunicationStatus(descriptors_result.Get())) {
    BLUETOOTH_LOG(DEBUG) << "Failed to get descriptors for characteristic "
                         << characteristic_attribute_handle << ".";
    std::move(callback_).Run(false);
    return;
  }

  ComPtr<IVectorView<GattDescriptor*>> descriptors;
  HRESULT hr = descriptors_result->get_Descriptors(&descriptors);
  if (FAILED(hr)) {
    BLUETOOTH_LOG(DEBUG) << "Getting descriptors failed: "
                         << logging::SystemErrorCodeToString(hr);
    std::move(callback_).Run(false);
    return;
  }

  DCHECK(!base::Contains(characteristic_to_descriptors_map_,
                         characteristic_attribute_handle));
  if (!GetAsVector(descriptors.Get(), &characteristic_to_descriptors_map_
                                          [characteristic_attribute_handle])) {
    std::move(callback_).Run(false);
    return;
  }

  RunCallbackIfDone();
}

void BluetoothGattDiscovererWinrt::RunCallbackIfDone() {
  DCHECK(callback_);
  if (service_to_characteristics_map_.size() == num_services_ &&
      characteristic_to_descriptors_map_.size() == num_characteristics_) {
    std::move(callback_).Run(true);
  }
}

}  // namespace device

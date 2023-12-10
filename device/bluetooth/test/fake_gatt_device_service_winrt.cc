// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/fake_gatt_device_service_winrt.h"

#include <wrl/client.h>

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/win/async_operation.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "device/bluetooth/test/fake_bluetooth_le_device_winrt.h"
#include "device/bluetooth/test/fake_gatt_characteristic_winrt.h"
#include "device/bluetooth/test/fake_gatt_characteristics_result_winrt.h"

namespace device {

namespace {

using ABI::Windows::Devices::Bluetooth::BluetoothCacheMode;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCharacteristic;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCharacteristicsResult;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattDeviceService;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattDeviceServicesResult;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::GattOpenStatus;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattOpenStatus_AccessDenied;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattOpenStatus_AlreadyOpened;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattOpenStatus_Success;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattSharingMode;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattSharingMode_SharedReadAndWrite;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::IGattSession;
using ABI::Windows::Devices::Enumeration::DeviceAccessStatus;
using ABI::Windows::Devices::Enumeration::IDeviceAccessInformation;
using ABI::Windows::Foundation::IAsyncOperation;
using ABI::Windows::Foundation::Collections::IVectorView;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Make;

}  // namespace

FakeGattDeviceServiceWinrt::FakeGattDeviceServiceWinrt(
    BluetoothTestWinrt* bluetooth_test_winrt,
    ComPtr<FakeBluetoothLEDeviceWinrt> fake_device,
    std::string_view uuid,
    uint16_t attribute_handle,
    bool allowed)
    : bluetooth_test_winrt_(bluetooth_test_winrt),
      fake_device_(std::move(fake_device)),
      uuid_(BluetoothUUID::GetCanonicalValueAsGUID(uuid)),
      attribute_handle_(attribute_handle),
      allowed_(allowed),
      characteristic_attribute_handle_(attribute_handle_) {
  fake_device_->AddReference();
}

FakeGattDeviceServiceWinrt::~FakeGattDeviceServiceWinrt() {
  fake_device_->RemoveReference();
}

HRESULT FakeGattDeviceServiceWinrt::GetCharacteristics(
    GUID characteristic_uuid,
    IVectorView<GattCharacteristic*>** value) {
  return E_NOTIMPL;
}

HRESULT FakeGattDeviceServiceWinrt::GetIncludedServices(
    GUID service_uuid,
    IVectorView<GattDeviceService*>** value) {
  return E_NOTIMPL;
}

HRESULT FakeGattDeviceServiceWinrt::get_DeviceId(HSTRING* value) {
  return E_NOTIMPL;
}

HRESULT FakeGattDeviceServiceWinrt::get_Uuid(GUID* value) {
  *value = uuid_;
  return S_OK;
}

HRESULT FakeGattDeviceServiceWinrt::get_AttributeHandle(uint16_t* value) {
  *value = attribute_handle_;
  return S_OK;
}

HRESULT FakeGattDeviceServiceWinrt::get_DeviceAccessInformation(
    IDeviceAccessInformation** value) {
  return E_NOTIMPL;
}

HRESULT FakeGattDeviceServiceWinrt::get_Session(IGattSession** value) {
  return E_NOTIMPL;
}

HRESULT FakeGattDeviceServiceWinrt::get_SharingMode(GattSharingMode* value) {
  return E_NOTIMPL;
}

HRESULT FakeGattDeviceServiceWinrt::RequestAccessAsync(
    IAsyncOperation<DeviceAccessStatus>** value) {
  return E_NOTIMPL;
}

HRESULT FakeGattDeviceServiceWinrt::OpenAsync(
    GattSharingMode sharing_mode,
    IAsyncOperation<GattOpenStatus>** operation) {
  if (sharing_mode != GattSharingMode_SharedReadAndWrite)
    return E_NOTIMPL;

  GattOpenStatus status;
  if (allowed_) {
    status = opened_ ? GattOpenStatus_AlreadyOpened : GattOpenStatus_Success;
    opened_ = true;
  } else {
    status = GattOpenStatus_AccessDenied;
  }

  auto async_op = Make<base::win::AsyncOperation<GattOpenStatus>>();
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(async_op->callback(), status));
  *operation = async_op.Detach();
  return S_OK;
}

HRESULT FakeGattDeviceServiceWinrt::GetCharacteristicsAsync(
    IAsyncOperation<GattCharacteristicsResult*>** operation) {
  // It has been observed that this method will implicitly call
  // OpenAsync(Exclusive) if the service has not been opened already. Catch
  // calls to an unopened service as we do not want to take an exclusive lock
  // on a service.
  if (!opened_)
    return E_NOTIMPL;

  auto async_op = Make<base::win::AsyncOperation<GattCharacteristicsResult*>>();
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(async_op->callback(),
                                Make<FakeGattCharacteristicsResultWinrt>(
                                    fake_characteristics_)));
  *operation = async_op.Detach();
  return S_OK;
}

HRESULT FakeGattDeviceServiceWinrt::GetCharacteristicsWithCacheModeAsync(
    BluetoothCacheMode cache_mode,
    IAsyncOperation<GattCharacteristicsResult*>** operation) {
  return E_NOTIMPL;
}

HRESULT FakeGattDeviceServiceWinrt::GetCharacteristicsForUuidAsync(
    GUID characteristic_uuid,
    IAsyncOperation<GattCharacteristicsResult*>** operation) {
  return E_NOTIMPL;
}

HRESULT FakeGattDeviceServiceWinrt::GetCharacteristicsForUuidWithCacheModeAsync(
    GUID characteristic_uuid,
    BluetoothCacheMode cache_mode,
    IAsyncOperation<GattCharacteristicsResult*>** operation) {
  return E_NOTIMPL;
}

HRESULT FakeGattDeviceServiceWinrt::GetIncludedServicesAsync(
    IAsyncOperation<GattDeviceServicesResult*>** operation) {
  return E_NOTIMPL;
}

HRESULT FakeGattDeviceServiceWinrt::GetIncludedServicesWithCacheModeAsync(
    BluetoothCacheMode cache_mode,
    IAsyncOperation<GattDeviceServicesResult*>** operation) {
  return E_NOTIMPL;
}

HRESULT FakeGattDeviceServiceWinrt::GetIncludedServicesForUuidAsync(
    GUID service_uuid,
    IAsyncOperation<GattDeviceServicesResult*>** operation) {
  return E_NOTIMPL;
}

HRESULT
FakeGattDeviceServiceWinrt::GetIncludedServicesForUuidWithCacheModeAsync(
    GUID service_uuid,
    BluetoothCacheMode cache_mode,
    IAsyncOperation<GattDeviceServicesResult*>** operation) {
  return E_NOTIMPL;
}

void FakeGattDeviceServiceWinrt::SimulateGattCharacteristic(
    std::string_view uuid,
    int properties) {
  // In order to ensure attribute handles are unique across the Gatt Server
  // we reserve sufficient address space for descriptors for each
  // characteristic. We allocate space for 32 descriptors, which should be
  // enough for tests.
  fake_characteristics_.push_back(Make<FakeGattCharacteristicWinrt>(
      bluetooth_test_winrt_, properties, uuid,
      characteristic_attribute_handle_ += 0x20));
}

}  // namespace device

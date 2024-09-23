// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/bluetooth/test/fake_gatt_descriptor_winrt.h"

#include <utility>

#include "base/win/async_operation.h"
#include "base/win/winrt_storage_util.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "device/bluetooth/test/bluetooth_test_win.h"
#include "device/bluetooth/test/fake_gatt_read_result_winrt.h"
#include "device/bluetooth/test/fake_gatt_write_result_winrt.h"

namespace device {

namespace {

using ABI::Windows::Devices::Bluetooth::BluetoothCacheMode;
using ABI::Windows::Devices::Bluetooth::BluetoothCacheMode_Uncached;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCommunicationStatus;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattProtectionLevel;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::GattReadResult;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattWriteResult;
using ABI::Windows::Foundation::IAsyncOperation;
using ABI::Windows::Storage::Streams::IBuffer;
using Microsoft::WRL::Make;

}  // namespace

FakeGattDescriptorWinrt::FakeGattDescriptorWinrt(
    BluetoothTestWinrt* bluetooth_test_winrt,
    std::string_view uuid,
    uint16_t attribute_handle)
    : bluetooth_test_winrt_(bluetooth_test_winrt),
      uuid_(BluetoothUUID::GetCanonicalValueAsGUID(uuid)),
      attribute_handle_(attribute_handle) {}

FakeGattDescriptorWinrt::~FakeGattDescriptorWinrt() = default;

HRESULT FakeGattDescriptorWinrt::get_ProtectionLevel(
    GattProtectionLevel* value) {
  return E_NOTIMPL;
}

HRESULT FakeGattDescriptorWinrt::put_ProtectionLevel(
    GattProtectionLevel value) {
  return E_NOTIMPL;
}

HRESULT FakeGattDescriptorWinrt::get_Uuid(GUID* value) {
  *value = uuid_;
  return S_OK;
}

HRESULT FakeGattDescriptorWinrt::get_AttributeHandle(uint16_t* value) {
  *value = attribute_handle_;
  return S_OK;
}

HRESULT FakeGattDescriptorWinrt::ReadValueAsync(
    IAsyncOperation<GattReadResult*>** value) {
  return E_NOTIMPL;
}

HRESULT FakeGattDescriptorWinrt::ReadValueWithCacheModeAsync(
    BluetoothCacheMode cache_mode,
    IAsyncOperation<GattReadResult*>** value) {
  if (cache_mode != BluetoothCacheMode_Uncached)
    return E_NOTIMPL;

  auto async_op = Make<base::win::AsyncOperation<GattReadResult*>>();
  DCHECK(!read_value_callback_);
  read_value_callback_ = async_op->callback();
  *value = async_op.Detach();
  bluetooth_test_winrt_->OnFakeBluetoothDescriptorReadValue();
  return S_OK;
}

HRESULT FakeGattDescriptorWinrt::WriteValueAsync(
    IBuffer* value,
    IAsyncOperation<GattCommunicationStatus>** async_op) {
  return E_NOTIMPL;
}

HRESULT FakeGattDescriptorWinrt::WriteValueWithResultAsync(
    IBuffer* value,
    IAsyncOperation<GattWriteResult*>** operation) {
  uint8_t* data;
  uint32_t size;
  base::win::GetPointerToBufferData(value, &data, &size);
  bluetooth_test_winrt_->OnFakeBluetoothDescriptorWriteValue(
      std::vector<uint8_t>(data, data + size));
  auto async_op = Make<base::win::AsyncOperation<GattWriteResult*>>();
  DCHECK(!write_value_callback_);
  write_value_callback_ = async_op->callback();
  *operation = async_op.Detach();
  return S_OK;
}

void FakeGattDescriptorWinrt::SimulateGattDescriptorRead(
    const std::vector<uint8_t>& data) {
  if (read_value_callback_)
    std::move(read_value_callback_).Run(Make<FakeGattReadResultWinrt>(data));
}

void FakeGattDescriptorWinrt::SimulateGattDescriptorReadError(
    BluetoothGattService::GattErrorCode error_code) {
  if (read_value_callback_) {
    std::move(read_value_callback_)
        .Run(Make<FakeGattReadResultWinrt>(error_code));
  }
}

void FakeGattDescriptorWinrt::SimulateGattDescriptorWrite() {
  if (write_value_callback_)
    std::move(write_value_callback_).Run(Make<FakeGattWriteResultWinrt>());
}

void FakeGattDescriptorWinrt::SimulateGattDescriptorWriteError(
    BluetoothGattService::GattErrorCode error_code) {
  if (write_value_callback_) {
    std::move(write_value_callback_)
        .Run(Make<FakeGattWriteResultWinrt>(error_code));
  }
}

}  // namespace device

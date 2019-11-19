// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/fake_gatt_characteristic_winrt.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/win/async_operation.h"
#include "base/win/winrt_storage_util.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "device/bluetooth/test/bluetooth_test_win.h"
#include "device/bluetooth/test/fake_gatt_descriptor_winrt.h"
#include "device/bluetooth/test/fake_gatt_descriptors_result_winrt.h"
#include "device/bluetooth/test/fake_gatt_read_result_winrt.h"
#include "device/bluetooth/test/fake_gatt_value_changed_event_args_winrt.h"
#include "device/bluetooth/test/fake_gatt_write_result_winrt.h"

namespace device {

namespace {

using ABI::Windows::Devices::Bluetooth::BluetoothCacheMode;
using ABI::Windows::Devices::Bluetooth::BluetoothCacheMode_Uncached;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCharacteristic;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCharacteristicProperties;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattClientCharacteristicConfigurationDescriptorValue;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCommunicationStatus;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCommunicationStatus_Success;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::GattDescriptor;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattDescriptorsResult;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattPresentationFormat;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattProtectionLevel;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattReadClientCharacteristicConfigurationDescriptorResult;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::GattReadResult;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattValueChangedEventArgs;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattWriteOption;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattWriteOption_WriteWithResponse;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattWriteResult;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    IGattReadResult;
using ABI::Windows::Foundation::IAsyncOperation;
using ABI::Windows::Foundation::ITypedEventHandler;
using ABI::Windows::Foundation::Collections::IVectorView;
using ABI::Windows::Storage::Streams::IBuffer;
using Microsoft::WRL::Make;

}  // namespace

FakeGattCharacteristicWinrt::FakeGattCharacteristicWinrt(
    BluetoothTestWinrt* bluetooth_test_winrt,
    int properties,
    base::StringPiece uuid,
    uint16_t attribute_handle)
    : bluetooth_test_winrt_(bluetooth_test_winrt),
      properties_(static_cast<GattCharacteristicProperties>(properties)),
      uuid_(BluetoothUUID::GetCanonicalValueAsGUID(uuid)),
      attribute_handle_(attribute_handle),
      last_descriptor_attribute_handle_(attribute_handle) {}

FakeGattCharacteristicWinrt::~FakeGattCharacteristicWinrt() = default;

HRESULT FakeGattCharacteristicWinrt::GetDescriptors(
    GUID descriptor_uuid,
    IVectorView<GattDescriptor*>** value) {
  return E_NOTIMPL;
}

HRESULT FakeGattCharacteristicWinrt::get_CharacteristicProperties(
    GattCharacteristicProperties* value) {
  *value = properties_;
  return S_OK;
}

HRESULT FakeGattCharacteristicWinrt::get_ProtectionLevel(
    GattProtectionLevel* value) {
  return E_NOTIMPL;
}

HRESULT FakeGattCharacteristicWinrt::put_ProtectionLevel(
    GattProtectionLevel value) {
  return E_NOTIMPL;
}

HRESULT FakeGattCharacteristicWinrt::get_UserDescription(HSTRING* value) {
  return E_NOTIMPL;
}

HRESULT FakeGattCharacteristicWinrt::get_Uuid(GUID* value) {
  *value = uuid_;
  return S_OK;
}

HRESULT FakeGattCharacteristicWinrt::get_AttributeHandle(uint16_t* value) {
  *value = attribute_handle_;
  return S_OK;
}

HRESULT FakeGattCharacteristicWinrt::get_PresentationFormats(
    IVectorView<GattPresentationFormat*>** value) {
  return E_NOTIMPL;
}

HRESULT FakeGattCharacteristicWinrt::ReadValueAsync(
    IAsyncOperation<GattReadResult*>** value) {
  return E_NOTIMPL;
}

HRESULT FakeGattCharacteristicWinrt::ReadValueWithCacheModeAsync(
    BluetoothCacheMode cache_mode,
    IAsyncOperation<GattReadResult*>** value) {
  if (cache_mode != BluetoothCacheMode_Uncached)
    return E_NOTIMPL;

  auto async_op = Make<base::win::AsyncOperation<GattReadResult*>>();
  DCHECK(!read_value_callback_);
  read_value_callback_ = async_op->callback();
  *value = async_op.Detach();
  bluetooth_test_winrt_->OnFakeBluetoothCharacteristicReadValue();
  return S_OK;
}

HRESULT FakeGattCharacteristicWinrt::WriteValueAsync(
    IBuffer* value,
    IAsyncOperation<GattCommunicationStatus>** async_op) {
  return E_NOTIMPL;
}

HRESULT FakeGattCharacteristicWinrt::WriteValueWithOptionAsync(
    IBuffer* value,
    GattWriteOption write_option,
    IAsyncOperation<GattCommunicationStatus>** async_op) {
  return E_NOTIMPL;
}

HRESULT FakeGattCharacteristicWinrt::
    ReadClientCharacteristicConfigurationDescriptorAsync(
        IAsyncOperation<
            GattReadClientCharacteristicConfigurationDescriptorResult*>**
            async_op) {
  return E_NOTIMPL;
}

HRESULT FakeGattCharacteristicWinrt::
    WriteClientCharacteristicConfigurationDescriptorAsync(
        GattClientCharacteristicConfigurationDescriptorValue
            client_characteristic_configuration_descriptor_value,
        IAsyncOperation<GattCommunicationStatus>** async_op) {
  return E_NOTIMPL;
}

HRESULT FakeGattCharacteristicWinrt::add_ValueChanged(
    ITypedEventHandler<GattCharacteristic*, GattValueChangedEventArgs*>*
        value_changed_handler,
    EventRegistrationToken* value_changed_event_cookie) {
  value_changed_handler_ = value_changed_handler;
  return S_OK;
}

HRESULT FakeGattCharacteristicWinrt::remove_ValueChanged(
    EventRegistrationToken value_changed_event_cookie) {
  value_changed_handler_.Reset();
  return S_OK;
}

HRESULT FakeGattCharacteristicWinrt::GetDescriptorsAsync(
    IAsyncOperation<GattDescriptorsResult*>** operation) {
  auto async_op = Make<base::win::AsyncOperation<GattDescriptorsResult*>>();
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(async_op->callback(),
                     Make<FakeGattDescriptorsResultWinrt>(fake_descriptors_)));
  *operation = async_op.Detach();
  return S_OK;
}

HRESULT FakeGattCharacteristicWinrt::GetDescriptorsWithCacheModeAsync(
    BluetoothCacheMode cache_mode,
    IAsyncOperation<GattDescriptorsResult*>** operation) {
  return E_NOTIMPL;
}

HRESULT FakeGattCharacteristicWinrt::GetDescriptorsForUuidAsync(
    GUID descriptor_uuid,
    IAsyncOperation<GattDescriptorsResult*>** operation) {
  return E_NOTIMPL;
}

HRESULT FakeGattCharacteristicWinrt::GetDescriptorsForUuidWithCacheModeAsync(
    GUID descriptor_uuid,
    BluetoothCacheMode cache_mode,
    IAsyncOperation<GattDescriptorsResult*>** operation) {
  return E_NOTIMPL;
}

HRESULT FakeGattCharacteristicWinrt::WriteValueWithResultAsync(
    IBuffer* value,
    IAsyncOperation<GattWriteResult*>** operation) {
  return E_NOTIMPL;
}

HRESULT FakeGattCharacteristicWinrt::WriteValueWithResultAndOptionAsync(
    IBuffer* value,
    GattWriteOption write_option,
    IAsyncOperation<GattWriteResult*>** operation) {
  uint8_t* data;
  uint32_t size;
  base::win::GetPointerToBufferData(value, &data, &size);
  bluetooth_test_winrt_->OnFakeBluetoothCharacteristicWriteValue(
      std::vector<uint8_t>(data, data + size));
  auto async_op = Make<base::win::AsyncOperation<GattWriteResult*>>();
  DCHECK(!write_value_callback_);
  if (write_option == GattWriteOption_WriteWithResponse) {
    write_value_callback_ = async_op->callback();
  } else {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(async_op->callback(), Make<FakeGattWriteResultWinrt>()));
  }
  *operation = async_op.Detach();
  return S_OK;
}

HRESULT FakeGattCharacteristicWinrt::
    WriteClientCharacteristicConfigurationDescriptorWithResultAsync(
        GattClientCharacteristicConfigurationDescriptorValue
            client_characteristic_configuration_descriptor_value,
        IAsyncOperation<GattWriteResult*>** operation) {
  bluetooth_test_winrt_->OnFakeBluetoothGattSetCharacteristicNotification(
      static_cast<BluetoothTestBase::NotifyValueState>(
          client_characteristic_configuration_descriptor_value));
  auto async_op = Make<base::win::AsyncOperation<GattWriteResult*>>();
  notify_session_callback_ = async_op->callback();
  *operation = async_op.Detach();
  return S_OK;
}

void FakeGattCharacteristicWinrt::SimulateGattCharacteristicRead(
    const std::vector<uint8_t>& data) {
  if (read_value_callback_)
    std::move(read_value_callback_).Run(Make<FakeGattReadResultWinrt>(data));
}

void FakeGattCharacteristicWinrt::SimulateGattCharacteristicReadError(
    BluetoothGattService::GattErrorCode error_code) {
  if (read_value_callback_) {
    std::move(read_value_callback_)
        .Run(Make<FakeGattReadResultWinrt>(error_code));
  }
}

void FakeGattCharacteristicWinrt::SimulateGattCharacteristicWrite() {
  if (write_value_callback_)
    std::move(write_value_callback_).Run(Make<FakeGattWriteResultWinrt>());
}

void FakeGattCharacteristicWinrt::SimulateGattCharacteristicWriteError(
    BluetoothGattService::GattErrorCode error_code) {
  if (write_value_callback_) {
    std::move(write_value_callback_)
        .Run(Make<FakeGattWriteResultWinrt>(error_code));
  }
}

void FakeGattCharacteristicWinrt::SimulateGattDescriptor(
    base::StringPiece uuid) {
  fake_descriptors_.push_back(Make<FakeGattDescriptorWinrt>(
      bluetooth_test_winrt_, uuid, ++last_descriptor_attribute_handle_));
}

void FakeGattCharacteristicWinrt::SimulateGattNotifySessionStarted() {
  std::move(notify_session_callback_).Run(Make<FakeGattWriteResultWinrt>());
}

void FakeGattCharacteristicWinrt::SimulateGattNotifySessionStartError(
    BluetoothGattService::GattErrorCode error_code) {
  std::move(notify_session_callback_)
      .Run(Make<FakeGattWriteResultWinrt>(error_code));
}

void FakeGattCharacteristicWinrt::SimulateGattNotifySessionStopped() {
  std::move(notify_session_callback_).Run(Make<FakeGattWriteResultWinrt>());
}

void FakeGattCharacteristicWinrt::SimulateGattNotifySessionStopError(
    BluetoothGattService::GattErrorCode error_code) {
  std::move(notify_session_callback_)
      .Run(Make<FakeGattWriteResultWinrt>(error_code));
}

void FakeGattCharacteristicWinrt::SimulateGattCharacteristicChanged(
    const std::vector<uint8_t>& value) {
  DCHECK(value_changed_handler_);
  value_changed_handler_->Invoke(
      this, Make<FakeGattValueChangedEventArgsWinrt>(value).Get());
}

}  // namespace device

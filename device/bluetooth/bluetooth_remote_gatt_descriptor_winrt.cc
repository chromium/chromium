// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_remote_gatt_descriptor_winrt.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/win/post_async_results.h"
#include "base/win/winrt_storage_util.h"
#include "device/bluetooth/bluetooth_remote_gatt_service_winrt.h"
#include "device/bluetooth/event_utils_winrt.h"

namespace device {

namespace {

using ABI::Windows::Devices::Bluetooth::BluetoothCacheMode_Uncached;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCommunicationStatus;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCommunicationStatus_Success;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::GattReadResult;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattWriteResult;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    IGattDescriptor;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    IGattDescriptor2;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    IGattReadResult;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    IGattReadResult2;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    IGattWriteResult;
using ABI::Windows::Foundation::IAsyncOperation;
using ABI::Windows::Storage::Streams::IBuffer;
using Microsoft::WRL::ComPtr;

}  // namespace

// static
std::unique_ptr<BluetoothRemoteGattDescriptorWinrt>
BluetoothRemoteGattDescriptorWinrt::Create(
    BluetoothRemoteGattCharacteristic* characteristic,
    ComPtr<IGattDescriptor> descriptor) {
  DCHECK(descriptor);
  GUID guid;
  HRESULT hr = descriptor->get_Uuid(&guid);
  if (FAILED(hr)) {
    VLOG(2) << "Getting UUID failed: " << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }

  uint16_t attribute_handle;
  hr = descriptor->get_AttributeHandle(&attribute_handle);
  if (FAILED(hr)) {
    VLOG(2) << "Getting AttributeHandle failed: "
            << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }

  return base::WrapUnique(new BluetoothRemoteGattDescriptorWinrt(
      characteristic, std::move(descriptor), BluetoothUUID(guid),
      attribute_handle));
}

BluetoothRemoteGattDescriptorWinrt::~BluetoothRemoteGattDescriptorWinrt() =
    default;

std::string BluetoothRemoteGattDescriptorWinrt::GetIdentifier() const {
  return identifier_;
}

BluetoothUUID BluetoothRemoteGattDescriptorWinrt::GetUUID() const {
  return uuid_;
}

BluetoothGattCharacteristic::Permissions
BluetoothRemoteGattDescriptorWinrt::GetPermissions() const {
  NOTIMPLEMENTED();
  return BluetoothGattCharacteristic::Permissions();
}

const std::vector<uint8_t>& BluetoothRemoteGattDescriptorWinrt::GetValue()
    const {
  return value_;
}

BluetoothRemoteGattCharacteristic*
BluetoothRemoteGattDescriptorWinrt::GetCharacteristic() const {
  return characteristic_;
}

void BluetoothRemoteGattDescriptorWinrt::ReadRemoteDescriptor(
    ValueCallback callback,
    ErrorCallback error_callback) {
  if (pending_read_callbacks_ || pending_write_callbacks_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(error_callback),
                       BluetoothRemoteGattService::GATT_ERROR_IN_PROGRESS));
    return;
  }

  ComPtr<IAsyncOperation<GattReadResult*>> read_value_op;
  HRESULT hr = descriptor_->ReadValueWithCacheModeAsync(
      BluetoothCacheMode_Uncached, &read_value_op);
  if (FAILED(hr)) {
    VLOG(2) << "GattDescriptor::ReadValueWithCacheModeAsync failed: "
            << logging::SystemErrorCodeToString(hr);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(error_callback),
                       BluetoothRemoteGattService::GATT_ERROR_FAILED));
    return;
  }

  hr = base::win::PostAsyncResults(
      std::move(read_value_op),
      base::BindOnce(&BluetoothRemoteGattDescriptorWinrt::OnReadValue,
                     weak_ptr_factory_.GetWeakPtr()));

  if (FAILED(hr)) {
    VLOG(2) << "PostAsyncResults failed: "
            << logging::SystemErrorCodeToString(hr);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(error_callback),
                       BluetoothRemoteGattService::GATT_ERROR_FAILED));
    return;
  }

  pending_read_callbacks_ = std::make_unique<PendingReadCallbacks>(
      std::move(callback), std::move(error_callback));
}

void BluetoothRemoteGattDescriptorWinrt::WriteRemoteDescriptor(
    const std::vector<uint8_t>& value,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  if (pending_read_callbacks_ || pending_write_callbacks_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(error_callback),
                       BluetoothRemoteGattService::GATT_ERROR_IN_PROGRESS));
    return;
  }

  ComPtr<IGattDescriptor2> descriptor_2;
  HRESULT hr = descriptor_.As(&descriptor_2);
  if (FAILED(hr)) {
    VLOG(2) << "As IGattDescriptor2 failed: "
            << logging::SystemErrorCodeToString(hr);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(error_callback),
                       BluetoothRemoteGattService::GATT_ERROR_FAILED));
    return;
  }

  ComPtr<IBuffer> buffer;
  hr = base::win::CreateIBufferFromData(value.data(), value.size(), &buffer);
  if (FAILED(hr)) {
    VLOG(2) << "base::win::CreateIBufferFromData failed: "
            << logging::SystemErrorCodeToString(hr);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(error_callback),
                       BluetoothRemoteGattService::GATT_ERROR_FAILED));
    return;
  }

  ComPtr<IAsyncOperation<GattWriteResult*>> write_value_op;
  hr = descriptor_2->WriteValueWithResultAsync(buffer.Get(), &write_value_op);
  if (FAILED(hr)) {
    VLOG(2) << "GattDescriptor::WriteValueWithResultAsync failed: "
            << logging::SystemErrorCodeToString(hr);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(error_callback),
                       BluetoothRemoteGattService::GATT_ERROR_FAILED));
    return;
  }

  hr = base::win::PostAsyncResults(
      std::move(write_value_op),
      base::BindOnce(
          &BluetoothRemoteGattDescriptorWinrt::OnWriteValueWithResult,
          weak_ptr_factory_.GetWeakPtr()));

  if (FAILED(hr)) {
    VLOG(2) << "PostAsyncResults failed: "
            << logging::SystemErrorCodeToString(hr);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(error_callback),
                       BluetoothRemoteGattService::GATT_ERROR_FAILED));
    return;
  }

  pending_write_callbacks_ = std::make_unique<PendingWriteCallbacks>(
      std::move(callback), std::move(error_callback));
}

IGattDescriptor* BluetoothRemoteGattDescriptorWinrt::GetDescriptorForTesting() {
  return descriptor_.Get();
}

BluetoothRemoteGattDescriptorWinrt::PendingReadCallbacks::PendingReadCallbacks(
    ValueCallback callback,
    ErrorCallback error_callback)
    : callback(std::move(callback)),
      error_callback(std::move(error_callback)) {}

BluetoothRemoteGattDescriptorWinrt::PendingReadCallbacks::
    ~PendingReadCallbacks() = default;

BluetoothRemoteGattDescriptorWinrt::PendingWriteCallbacks::
    PendingWriteCallbacks(base::OnceClosure callback,
                          ErrorCallback error_callback)
    : callback(std::move(callback)),
      error_callback(std::move(error_callback)) {}

BluetoothRemoteGattDescriptorWinrt::PendingWriteCallbacks::
    ~PendingWriteCallbacks() = default;

BluetoothRemoteGattDescriptorWinrt::BluetoothRemoteGattDescriptorWinrt(
    BluetoothRemoteGattCharacteristic* characteristic,
    Microsoft::WRL::ComPtr<ABI::Windows::Devices::Bluetooth::
                               GenericAttributeProfile::IGattDescriptor>
        descriptor,
    BluetoothUUID uuid,
    uint16_t attribute_handle)
    : characteristic_(characteristic),
      descriptor_(std::move(descriptor)),
      uuid_(std::move(uuid)),
      identifier_(base::StringPrintf("%s/%s_%04x",
                                     characteristic_->GetIdentifier().c_str(),
                                     uuid_.value().c_str(),
                                     attribute_handle)) {}

void BluetoothRemoteGattDescriptorWinrt::OnReadValue(
    ComPtr<IGattReadResult> read_result) {
  DCHECK(pending_read_callbacks_);
  auto pending_read_callbacks = std::move(pending_read_callbacks_);

  if (!read_result) {
    std::move(pending_read_callbacks->error_callback)
        .Run(BluetoothGattService::GATT_ERROR_FAILED);
    return;
  }

  GattCommunicationStatus status;
  HRESULT hr = read_result->get_Status(&status);
  if (FAILED(hr)) {
    VLOG(2) << "Getting GATT Communication Status failed: "
            << logging::SystemErrorCodeToString(hr);
    std::move(pending_read_callbacks->error_callback)
        .Run(BluetoothGattService::GATT_ERROR_FAILED);
    return;
  }

  if (status != GattCommunicationStatus_Success) {
    VLOG(2) << "Unexpected GattCommunicationStatus: " << status;
    ComPtr<IGattReadResult2> read_result_2;
    hr = read_result.As(&read_result_2);
    if (FAILED(hr)) {
      VLOG(2) << "As IGattReadResult2 failed: "
              << logging::SystemErrorCodeToString(hr);
      std::move(pending_read_callbacks->error_callback)
          .Run(BluetoothGattService::GATT_ERROR_FAILED);
      return;
    }

    std::move(pending_read_callbacks->error_callback)
        .Run(BluetoothRemoteGattServiceWinrt::GetGattErrorCode(
            read_result_2.Get()));
    return;
  }

  ComPtr<IBuffer> value;
  hr = read_result->get_Value(&value);
  if (FAILED(hr)) {
    VLOG(2) << "Getting Descriptor Value failed: "
            << logging::SystemErrorCodeToString(hr);
    std::move(pending_read_callbacks->error_callback)
        .Run(BluetoothGattService::GATT_ERROR_FAILED);
    return;
  }

  uint8_t* data = nullptr;
  uint32_t length = 0;
  hr = base::win::GetPointerToBufferData(value.Get(), &data, &length);
  if (FAILED(hr)) {
    VLOG(2) << "Getting Pointer To Buffer Data failed: "
            << logging::SystemErrorCodeToString(hr);
    std::move(pending_read_callbacks->error_callback)
        .Run(BluetoothGattService::GATT_ERROR_FAILED);
    return;
  }

  value_.assign(data, data + length);
  std::move(pending_read_callbacks->callback).Run(value_);
}

void BluetoothRemoteGattDescriptorWinrt::OnWriteValueWithResult(
    ComPtr<IGattWriteResult> write_result) {
  DCHECK(pending_write_callbacks_);
  auto pending_write_callbacks = std::move(pending_write_callbacks_);

  if (!write_result) {
    std::move(pending_write_callbacks->error_callback)
        .Run(BluetoothGattService::GATT_ERROR_FAILED);
    return;
  }

  GattCommunicationStatus status;
  HRESULT hr = write_result->get_Status(&status);
  if (FAILED(hr)) {
    VLOG(2) << "Getting GATT Communication Status failed: "
            << logging::SystemErrorCodeToString(hr);
    std::move(pending_write_callbacks->error_callback)
        .Run(BluetoothGattService::GATT_ERROR_FAILED);
    return;
  }

  if (status != GattCommunicationStatus_Success) {
    VLOG(2) << "Unexpected GattCommunicationStatus: " << status;
    std::move(pending_write_callbacks->error_callback)
        .Run(BluetoothRemoteGattServiceWinrt::GetGattErrorCode(
            write_result.Get()));
    return;
  }

  std::move(pending_write_callbacks->callback).Run();
}

}  // namespace device

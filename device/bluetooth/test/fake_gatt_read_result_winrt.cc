// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/fake_gatt_read_result_winrt.h"

#include <wrl/client.h>

#include <utility>

#include "base/win/reference.h"
#include "base/win/winrt_storage_util.h"
#include "device/bluetooth/bluetooth_remote_gatt_service_winrt.h"

namespace {

using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCommunicationStatus;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCommunicationStatus_ProtocolError;
using ABI::Windows::Foundation::IReference;
using ABI::Windows::Storage::Streams::IBuffer;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Make;
}  // namespace

namespace device {

FakeGattReadResultWinrt::FakeGattReadResultWinrt(
    BluetoothGattService::GattErrorCode error_code)
    : status_(GattCommunicationStatus_ProtocolError),
      protocol_error_(
          BluetoothRemoteGattServiceWinrt::ToProtocolError(error_code)) {}

FakeGattReadResultWinrt::FakeGattReadResultWinrt(std::vector<uint8_t> data)
    : data_(std::move(data)) {}

FakeGattReadResultWinrt::~FakeGattReadResultWinrt() = default;

HRESULT FakeGattReadResultWinrt::get_Status(GattCommunicationStatus* value) {
  *value = status_;
  return S_OK;
}

HRESULT FakeGattReadResultWinrt::get_Value(IBuffer** value) {
  ComPtr<IBuffer> buffer;
  HRESULT hr =
      base::win::CreateIBufferFromData(data_.data(), data_.size(), &buffer);
  if (FAILED(hr))
    return hr;

  return buffer.CopyTo(value);
}

HRESULT FakeGattReadResultWinrt::get_ProtocolError(
    IReference<uint8_t>** value) {
  return Make<base::win::Reference<uint8_t>>(protocol_error_).CopyTo(value);
}

}  // namespace device

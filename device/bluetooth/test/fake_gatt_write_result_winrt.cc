// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/fake_gatt_write_result_winrt.h"

#include <wrl/client.h>

#include "base/win/reference.h"
#include "device/bluetooth/bluetooth_remote_gatt_service_winrt.h"

namespace {

using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCommunicationStatus;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCommunicationStatus_ProtocolError;
using ABI::Windows::Foundation::IReference;
using Microsoft::WRL::Make;

}  // namespace

namespace device {

FakeGattWriteResultWinrt::FakeGattWriteResultWinrt() = default;

FakeGattWriteResultWinrt::FakeGattWriteResultWinrt(
    BluetoothGattService::GattErrorCode error_code)
    : status_(GattCommunicationStatus_ProtocolError),
      protocol_error_(
          BluetoothRemoteGattServiceWinrt::ToProtocolError(error_code)) {}

FakeGattWriteResultWinrt::~FakeGattWriteResultWinrt() = default;

HRESULT FakeGattWriteResultWinrt::get_Status(GattCommunicationStatus* value) {
  *value = status_;
  return S_OK;
}

HRESULT FakeGattWriteResultWinrt::get_ProtocolError(
    IReference<uint8_t>** value) {
  return Make<base::win::Reference<uint8_t>>(protocol_error_).CopyTo(value);
}

}  // namespace device

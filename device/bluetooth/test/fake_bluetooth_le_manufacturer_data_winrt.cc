// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/fake_bluetooth_le_manufacturer_data_winrt.h"

#include <utility>

#include "base/win/winrt_storage_util.h"

namespace device {

namespace {

using ABI::Windows::Devices::Bluetooth::Advertisement::
    IBluetoothLEManufacturerData;
using ABI::Windows::Storage::Streams::IBuffer;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Make;

}  // namespace

FakeBluetoothLEManufacturerData::FakeBluetoothLEManufacturerData(
    uint16_t company_id,
    std::vector<uint8_t> data)
    : company_id_(company_id) {
  base::win::CreateIBufferFromData(data.data(),
                                   static_cast<uint32_t>(data.size()), &data_);
}

FakeBluetoothLEManufacturerData::FakeBluetoothLEManufacturerData(
    uint16_t company_id,
    ComPtr<IBuffer> data)
    : company_id_(company_id), data_(std::move(data)) {}

FakeBluetoothLEManufacturerData::~FakeBluetoothLEManufacturerData() = default;

HRESULT FakeBluetoothLEManufacturerData::get_CompanyId(uint16_t* value) {
  *value = company_id_;
  return S_OK;
}

HRESULT FakeBluetoothLEManufacturerData::put_CompanyId(uint16_t value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEManufacturerData::get_Data(IBuffer** value) {
  return data_.CopyTo(value);
}

HRESULT FakeBluetoothLEManufacturerData::put_Data(IBuffer* value) {
  return E_NOTIMPL;
}

FakeBluetoothLEManufacturerDataFactory::
    FakeBluetoothLEManufacturerDataFactory() = default;

FakeBluetoothLEManufacturerDataFactory::
    ~FakeBluetoothLEManufacturerDataFactory() = default;

HRESULT FakeBluetoothLEManufacturerDataFactory::Create(
    uint16_t company_id,
    IBuffer* data,
    IBluetoothLEManufacturerData** value) {
  return Make<FakeBluetoothLEManufacturerData>(company_id, data).CopyTo(value);
}

}  // namespace device

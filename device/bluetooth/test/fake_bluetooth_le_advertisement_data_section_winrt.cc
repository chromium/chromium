// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/fake_bluetooth_le_advertisement_data_section_winrt.h"

#include <wrl/client.h>

#include <utility>

#include "base/win/winrt_storage_util.h"

namespace device {

namespace {

using ABI::Windows::Storage::Streams::IBuffer;
using Microsoft::WRL::ComPtr;

}  // namespace

FakeBluetoothLEAdvertisementDataSectionWinrt::
    FakeBluetoothLEAdvertisementDataSectionWinrt(std::vector<uint8_t> data)
    : data_(std::move(data)) {}

FakeBluetoothLEAdvertisementDataSectionWinrt::
    ~FakeBluetoothLEAdvertisementDataSectionWinrt() = default;

HRESULT FakeBluetoothLEAdvertisementDataSectionWinrt::get_DataType(
    uint8_t* value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEAdvertisementDataSectionWinrt::put_DataType(
    uint8_t value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEAdvertisementDataSectionWinrt::get_Data(
    IBuffer** value) {
  ComPtr<IBuffer> buffer;
  HRESULT hr = base::win::CreateIBufferFromData(
      data_.data(), static_cast<uint32_t>(data_.size()), &buffer);
  return SUCCEEDED(hr) ? buffer.CopyTo(value) : hr;
}

HRESULT FakeBluetoothLEAdvertisementDataSectionWinrt::put_Data(IBuffer* value) {
  return E_NOTIMPL;
}

}  // namespace device

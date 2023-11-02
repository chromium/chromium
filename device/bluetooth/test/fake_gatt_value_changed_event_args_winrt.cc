// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/fake_gatt_value_changed_event_args_winrt.h"

#include <wrl/client.h>

#include <utility>

#include "base/win/winrt_storage_util.h"

namespace device {

namespace {

using ABI::Windows::Foundation::DateTime;
using ABI::Windows::Storage::Streams::IBuffer;
using Microsoft::WRL::ComPtr;

}  // namespace

FakeGattValueChangedEventArgsWinrt::FakeGattValueChangedEventArgsWinrt(
    std::vector<uint8_t> value)
    : value_(std::move(value)) {}

FakeGattValueChangedEventArgsWinrt::~FakeGattValueChangedEventArgsWinrt() =
    default;

HRESULT
FakeGattValueChangedEventArgsWinrt::get_CharacteristicValue(IBuffer** value) {
  ComPtr<IBuffer> buffer;
  HRESULT hr =
      base::win::CreateIBufferFromData(value_.data(), value_.size(), &buffer);
  return SUCCEEDED(hr) ? buffer.CopyTo(value) : hr;
}

HRESULT FakeGattValueChangedEventArgsWinrt::get_Timestamp(DateTime* timestamp) {
  return E_NOTIMPL;
}

}  // namespace device

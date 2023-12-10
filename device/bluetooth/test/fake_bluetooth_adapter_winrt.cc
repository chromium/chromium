// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/fake_bluetooth_adapter_winrt.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/task/sequenced_task_runner.h"
#include "base/win/async_operation.h"
#include "device/bluetooth/test/bluetooth_test.h"

namespace device {

namespace {

// In order to avoid a name clash with device::BluetoothAdapter we need this
// auxiliary namespace.
namespace uwp {
using ABI::Windows::Devices::Bluetooth::BluetoothAdapter;
}  // namespace uwp
using ABI::Windows::Devices::Bluetooth::IBluetoothAdapter;
using ABI::Windows::Devices::Radios::Radio;
using ABI::Windows::Foundation::IAsyncOperation;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Make;

}  // namespace

FakeBluetoothAdapterWinrt::FakeBluetoothAdapterWinrt(
    std::string_view address,
    Microsoft::WRL::ComPtr<ABI::Windows::Devices::Radios::IRadio> radio)
    : raw_address_(ToRawBluetoothAddress(address)), radio_(std::move(radio)) {}

FakeBluetoothAdapterWinrt::~FakeBluetoothAdapterWinrt() = default;

// static
uint64_t FakeBluetoothAdapterWinrt::ToRawBluetoothAddress(
    std::string_view address) {
  uint64_t raw_address;
  const bool result = base::HexStringToUInt64(
      base::StrCat(base::SplitStringPiece(address, ":", base::TRIM_WHITESPACE,
                                          base::SPLIT_WANT_ALL)),
      &raw_address);
  DCHECK(result);
  return raw_address;
}

HRESULT FakeBluetoothAdapterWinrt::get_DeviceId(HSTRING* value) {
  // The actual device id does not matter for testing, as long as this method
  // returns a success code.
  return S_OK;
}

HRESULT FakeBluetoothAdapterWinrt::get_BluetoothAddress(UINT64* value) {
  *value = raw_address_;
  return S_OK;
}

HRESULT FakeBluetoothAdapterWinrt::get_IsClassicSupported(boolean* value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothAdapterWinrt::get_IsLowEnergySupported(boolean* value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothAdapterWinrt::get_IsPeripheralRoleSupported(
    boolean* value) {
  *value = true;
  return S_OK;
}

HRESULT FakeBluetoothAdapterWinrt::get_IsCentralRoleSupported(boolean* value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothAdapterWinrt::get_IsAdvertisementOffloadSupported(
    boolean* value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothAdapterWinrt::GetRadioAsync(
    IAsyncOperation<Radio*>** operation) {
  auto async_op = Make<base::win::AsyncOperation<Radio*>>();
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(async_op->callback(), radio_));
  *operation = async_op.Detach();
  return S_OK;
}

FakeBluetoothAdapterStaticsWinrt::FakeBluetoothAdapterStaticsWinrt(
    ComPtr<IBluetoothAdapter> default_adapter)
    : default_adapter_(std::move(default_adapter)) {}

FakeBluetoothAdapterStaticsWinrt::~FakeBluetoothAdapterStaticsWinrt() = default;

HRESULT FakeBluetoothAdapterStaticsWinrt::GetDeviceSelector(HSTRING* result) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothAdapterStaticsWinrt::FromIdAsync(
    HSTRING device_id,
    IAsyncOperation<uwp::BluetoothAdapter*>** operation) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothAdapterStaticsWinrt::GetDefaultAsync(
    IAsyncOperation<uwp::BluetoothAdapter*>** operation) {
  // Here we mimick production code and do not return an error code if no
  // default adapter is present. Just as production code, the async operation
  // completes successfully and returns a nullptr as adapter in this case.
  auto async_op = Make<base::win::AsyncOperation<uwp::BluetoothAdapter*>>();
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(async_op->callback(), default_adapter_));
  *operation = async_op.Detach();
  return S_OK;
}

}  // namespace device

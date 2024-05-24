// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/fake_device_information_winrt.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/win/async_operation.h"
#include "base/win/scoped_hstring.h"
#include "device/bluetooth/test/fake_device_watcher_winrt.h"

namespace device {

namespace {

using ABI::Windows::Devices::Enumeration::DeviceClass;
using ABI::Windows::Devices::Enumeration::DeviceInformation;
using ABI::Windows::Devices::Enumeration::DeviceInformationCollection;
using ABI::Windows::Devices::Enumeration::DeviceInformationKind;
using ABI::Windows::Devices::Enumeration::DeviceThumbnail;
using ABI::Windows::Devices::Enumeration::IDeviceInformation;
using ABI::Windows::Devices::Enumeration::IDeviceInformationPairing;
using ABI::Windows::Devices::Enumeration::IDeviceInformationUpdate;
using ABI::Windows::Devices::Enumeration::IDeviceWatcher;
using ABI::Windows::Devices::Enumeration::IEnclosureLocation;
using ABI::Windows::Foundation::Collections::IIterable;
using ABI::Windows::Foundation::Collections::IMapView;
using ABI::Windows::Foundation::IAsyncOperation;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Make;

}  // namespace

FakeDeviceInformationWinrt::FakeDeviceInformationWinrt() = default;

FakeDeviceInformationWinrt::FakeDeviceInformationWinrt(const char* name)
    : name_(name) {}

FakeDeviceInformationWinrt::FakeDeviceInformationWinrt(std::string name)
    : name_(std::move(name)) {}

FakeDeviceInformationWinrt::FakeDeviceInformationWinrt(
    ComPtr<IDeviceInformationPairing> pairing)
    : pairing_(std::move(pairing)) {}

FakeDeviceInformationWinrt::~FakeDeviceInformationWinrt() = default;

HRESULT FakeDeviceInformationWinrt::get_Id(HSTRING* value) {
  return E_NOTIMPL;
}

HRESULT FakeDeviceInformationWinrt::get_Name(HSTRING* value) {
  *value = base::win::ScopedHString::Create(name_).release();
  return S_OK;
}

HRESULT FakeDeviceInformationWinrt::get_IsEnabled(boolean* value) {
  return E_NOTIMPL;
}

HRESULT FakeDeviceInformationWinrt::get_IsDefault(boolean* value) {
  return E_NOTIMPL;
}

HRESULT FakeDeviceInformationWinrt::get_EnclosureLocation(
    IEnclosureLocation** value) {
  return E_NOTIMPL;
}

HRESULT FakeDeviceInformationWinrt::get_Properties(
    IMapView<HSTRING, IInspectable*>** value) {
  return E_NOTIMPL;
}

HRESULT FakeDeviceInformationWinrt::Update(
    IDeviceInformationUpdate* update_info) {
  return E_NOTIMPL;
}

HRESULT FakeDeviceInformationWinrt::GetThumbnailAsync(
    IAsyncOperation<DeviceThumbnail*>** async_op) {
  return E_NOTIMPL;
}

HRESULT FakeDeviceInformationWinrt::GetGlyphThumbnailAsync(
    IAsyncOperation<DeviceThumbnail*>** async_op) {
  return E_NOTIMPL;
}

HRESULT FakeDeviceInformationWinrt::get_Kind(DeviceInformationKind* value) {
  return E_NOTIMPL;
}

HRESULT FakeDeviceInformationWinrt::get_Pairing(
    IDeviceInformationPairing** value) {
  return pairing_.CopyTo(value);
}

FakeDeviceInformationStaticsWinrt::FakeDeviceInformationStaticsWinrt(
    ComPtr<IDeviceInformation> device_information)
    : device_information_(std::move(device_information)) {}

FakeDeviceInformationStaticsWinrt::~FakeDeviceInformationStaticsWinrt() =
    default;

HRESULT FakeDeviceInformationStaticsWinrt::CreateFromIdAsync(
    HSTRING device_id,
    IAsyncOperation<DeviceInformation*>** async_op) {
  auto operation = Make<base::win::AsyncOperation<DeviceInformation*>>();
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(operation->callback(), device_information_));
  *async_op = operation.Detach();
  return S_OK;
}

HRESULT
FakeDeviceInformationStaticsWinrt::CreateFromIdAsyncAdditionalProperties(
    HSTRING device_id,
    IIterable<HSTRING>* additional_properties,
    IAsyncOperation<DeviceInformation*>** async_op) {
  return E_NOTIMPL;
}

HRESULT FakeDeviceInformationStaticsWinrt::FindAllAsync(
    IAsyncOperation<DeviceInformationCollection*>** async_op) {
  return E_NOTIMPL;
}

HRESULT FakeDeviceInformationStaticsWinrt::FindAllAsyncDeviceClass(
    DeviceClass device_class,
    IAsyncOperation<DeviceInformationCollection*>** async_op) {
  return E_NOTIMPL;
}

HRESULT FakeDeviceInformationStaticsWinrt::FindAllAsyncAqsFilter(
    HSTRING aqs_filter,
    IAsyncOperation<DeviceInformationCollection*>** async_op) {
  return E_NOTIMPL;
}

HRESULT
FakeDeviceInformationStaticsWinrt::FindAllAsyncAqsFilterAndAdditionalProperties(
    HSTRING aqs_filter,
    IIterable<HSTRING>* additional_properties,
    IAsyncOperation<DeviceInformationCollection*>** async_op) {
  return E_NOTIMPL;
}

HRESULT FakeDeviceInformationStaticsWinrt::CreateWatcher(
    IDeviceWatcher** watcher) {
  return E_NOTIMPL;
}

HRESULT FakeDeviceInformationStaticsWinrt::CreateWatcherDeviceClass(
    DeviceClass device_class,
    IDeviceWatcher** watcher) {
  return E_NOTIMPL;
}

HRESULT FakeDeviceInformationStaticsWinrt::CreateWatcherAqsFilter(
    HSTRING aqs_filter,
    IDeviceWatcher** watcher) {
  return Make<FakeDeviceWatcherWinrt>().CopyTo(watcher);
}

HRESULT FakeDeviceInformationStaticsWinrt::
    CreateWatcherAqsFilterAndAdditionalProperties(
        HSTRING aqs_filter,
        IIterable<HSTRING>* additional_properties,
        IDeviceWatcher** watcher) {
  return E_NOTIMPL;
}

}  // namespace device

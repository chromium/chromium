// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/fake_device_watcher_winrt.h"

#include <utility>

namespace device {

namespace {

using ABI::Windows::Devices::Enumeration::DeviceInformation;
using ABI::Windows::Devices::Enumeration::DeviceInformationUpdate;
using ABI::Windows::Devices::Enumeration::DeviceWatcher;
using ABI::Windows::Devices::Enumeration::DeviceWatcherStatus;
using ABI::Windows::Foundation::ITypedEventHandler;

}  // namespace

FakeDeviceWatcherWinrt::FakeDeviceWatcherWinrt() = default;

FakeDeviceWatcherWinrt::~FakeDeviceWatcherWinrt() = default;

HRESULT FakeDeviceWatcherWinrt::add_Added(
    ITypedEventHandler<DeviceWatcher*, DeviceInformation*>* handler,
    EventRegistrationToken* token) {
  added_handler_ = handler;
  return S_OK;
}

HRESULT FakeDeviceWatcherWinrt::remove_Added(EventRegistrationToken token) {
  added_handler_.Reset();
  return S_OK;
}

HRESULT FakeDeviceWatcherWinrt::add_Updated(
    ITypedEventHandler<DeviceWatcher*, DeviceInformationUpdate*>* handler,
    EventRegistrationToken* token) {
  return E_NOTIMPL;
}

HRESULT FakeDeviceWatcherWinrt::remove_Updated(EventRegistrationToken token) {
  return E_NOTIMPL;
}

HRESULT FakeDeviceWatcherWinrt::add_Removed(
    ITypedEventHandler<DeviceWatcher*, DeviceInformationUpdate*>* handler,
    EventRegistrationToken* token) {
  removed_handler_ = handler;
  return S_OK;
}

HRESULT FakeDeviceWatcherWinrt::remove_Removed(EventRegistrationToken token) {
  removed_handler_.Reset();
  return S_OK;
}

HRESULT FakeDeviceWatcherWinrt::add_EnumerationCompleted(
    ITypedEventHandler<DeviceWatcher*, IInspectable*>* handler,
    EventRegistrationToken* token) {
  enumerated_handler_ = handler;
  return S_OK;
}

HRESULT FakeDeviceWatcherWinrt::remove_EnumerationCompleted(
    EventRegistrationToken token) {
  removed_handler_.Reset();
  return S_OK;
}

HRESULT FakeDeviceWatcherWinrt::add_Stopped(
    ITypedEventHandler<DeviceWatcher*, IInspectable*>* handler,
    EventRegistrationToken* token) {
  return E_NOTIMPL;
}

HRESULT FakeDeviceWatcherWinrt::remove_Stopped(EventRegistrationToken token) {
  return E_NOTIMPL;
}

HRESULT FakeDeviceWatcherWinrt::get_Status(DeviceWatcherStatus* status) {
  return E_NOTIMPL;
}

HRESULT FakeDeviceWatcherWinrt::Start() {
  if (enumerated_handler_)
    enumerated_handler_->Invoke(this, nullptr);
  return S_OK;
}

HRESULT FakeDeviceWatcherWinrt::Stop() {
  return S_OK;
}

void FakeDeviceWatcherWinrt::SimulateAdapterPoweredOn() {
  if (!std::exchange(has_powered_radio_, true) && added_handler_)
    added_handler_->Invoke(this, nullptr);
}

void FakeDeviceWatcherWinrt::SimulateAdapterPoweredOff() {
  if (std::exchange(has_powered_radio_, false) && removed_handler_)
    removed_handler_->Invoke(this, nullptr);
}

}  // namespace device

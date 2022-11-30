// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/fake_device_pairing_result_winrt.h"

namespace {

using ABI::Windows::Devices::Enumeration::DevicePairingProtectionLevel;
using ABI::Windows::Devices::Enumeration::DevicePairingResultStatus;

}  // namespace

namespace device {

FakeDevicePairingResultWinrt::FakeDevicePairingResultWinrt(
    DevicePairingResultStatus status)
    : status_(status) {}

FakeDevicePairingResultWinrt::~FakeDevicePairingResultWinrt() = default;

HRESULT FakeDevicePairingResultWinrt::get_Status(
    DevicePairingResultStatus* status) {
  *status = status_;
  return S_OK;
}

HRESULT FakeDevicePairingResultWinrt::get_ProtectionLevelUsed(
    DevicePairingProtectionLevel* value) {
  return E_NOTIMPL;
}

}  // namespace device

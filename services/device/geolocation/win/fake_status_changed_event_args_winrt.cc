// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/win/fake_status_changed_event_args_winrt.h"

namespace device {
namespace {
using ABI::Windows::Devices::Geolocation::PositionStatus;
}

FakeStatusChangedEventArgs::FakeStatusChangedEventArgs(
    PositionStatus position_status)
    : position_status_(position_status) {}

FakeStatusChangedEventArgs::~FakeStatusChangedEventArgs() = default;

IFACEMETHODIMP FakeStatusChangedEventArgs::get_Status(PositionStatus* value) {
  *value = position_status_;
  return S_OK;
}

}  // namespace device
// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/fake_device_management_error_details.h"

FakeDeviceManagementErrorDetails::FakeDeviceManagementErrorDetails() = default;
FakeDeviceManagementErrorDetails::~FakeDeviceManagementErrorDetails() = default;

std::unique_ptr<DeviceManagementErrorDetails>
FakeDeviceManagementErrorDetails::Clone() const {
  return std::make_unique<FakeDeviceManagementErrorDetails>();
}

bool FakeDeviceManagementErrorDetails::Equals(
    const DeviceManagementErrorDetails& other) const {
  return true;
}

bool FakeDeviceManagementErrorDetails::IsUserActionable() const {
  return false;
}

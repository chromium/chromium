// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_FAKE_DEVICE_MANAGEMENT_ERROR_DETAILS_H_
#define GOOGLE_APIS_GAIA_FAKE_DEVICE_MANAGEMENT_ERROR_DETAILS_H_

#include <memory>

#include "google_apis/gaia/device_management_error_details.h"

class FakeDeviceManagementErrorDetails : public DeviceManagementErrorDetails {
 public:
  FakeDeviceManagementErrorDetails();
  ~FakeDeviceManagementErrorDetails() override;

  std::unique_ptr<DeviceManagementErrorDetails> Clone() const override;
  bool Equals(const DeviceManagementErrorDetails& other) const override;
  bool IsUserActionable() const override;
};

#endif  // GOOGLE_APIS_GAIA_FAKE_DEVICE_MANAGEMENT_ERROR_DETAILS_H_

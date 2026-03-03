// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_DEVICE_MANAGEMENT_ERROR_DETAILS_H_
#define GOOGLE_APIS_GAIA_DEVICE_MANAGEMENT_ERROR_DETAILS_H_

#include <memory>

#include "base/component_export.h"

// Abstract base class for platform-specific device management error details.
// This class is intended to be implemented by platform-specific code
// to support device management errors within the
// cross-platform GoogleServiceAuthError class.

// Platform-specific implementations should:
//   Inherit from DeviceManagementErrorDetails.
//   Store any platform-specific data necessary to handle and display the
//   error
class COMPONENT_EXPORT(GOOGLE_APIS) DeviceManagementErrorDetails {
 public:
  virtual ~DeviceManagementErrorDetails();

  // Create a deep copy of the platform-specific details.
  virtual std::unique_ptr<DeviceManagementErrorDetails> Clone() const = 0;

  // Compare the platform-specific data with another instance.
  virtual bool Equals(const DeviceManagementErrorDetails& other) const = 0;

  // Indicate if the error requires user interaction to resolve.
  virtual bool IsUserActionable() const = 0;

 protected:
  DeviceManagementErrorDetails();
  DeviceManagementErrorDetails(const DeviceManagementErrorDetails&);
  DeviceManagementErrorDetails& operator=(const DeviceManagementErrorDetails&);
};

#endif  // GOOGLE_APIS_GAIA_DEVICE_MANAGEMENT_ERROR_DETAILS_H_

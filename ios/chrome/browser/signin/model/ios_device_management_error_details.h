// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_IOS_DEVICE_MANAGEMENT_ERROR_DETAILS_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_IOS_DEVICE_MANAGEMENT_ERROR_DETAILS_H_

#include <memory>

#include "google_apis/gaia/device_management_error_details.h"

@class NSDictionary;

// iOS implementation of DeviceManagementErrorDetails. It holds the user_info
// dictionary from the original NSError and whether the error is user
// actionable.
class IOSDeviceManagementErrorDetails : public DeviceManagementErrorDetails {
 public:
  explicit IOSDeviceManagementErrorDetails(NSDictionary* user_info,
                                           bool is_user_actionable);
  ~IOSDeviceManagementErrorDetails() override;

  IOSDeviceManagementErrorDetails(const IOSDeviceManagementErrorDetails&) =
      delete;
  IOSDeviceManagementErrorDetails& operator=(
      const IOSDeviceManagementErrorDetails&) = delete;

  std::unique_ptr<DeviceManagementErrorDetails> Clone() const override;
  bool Equals(const DeviceManagementErrorDetails& other) const override;
  bool IsUserActionable() const override;

  NSDictionary* GetNsUserInfo() const { return ns_user_info_; }

 private:
  NSDictionary* __strong ns_user_info_;
  const bool is_user_actionable_;
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_IOS_DEVICE_MANAGEMENT_ERROR_DETAILS_H_

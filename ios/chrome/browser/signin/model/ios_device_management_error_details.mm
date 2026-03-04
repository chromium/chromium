// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#import "ios/chrome/browser/signin/model/ios_device_management_error_details.h"

#import <Foundation/Foundation.h>

#import <memory>

#import "base/check.h"

IOSDeviceManagementErrorDetails::IOSDeviceManagementErrorDetails(
    NSDictionary* user_info,
    bool is_user_actionable)
    : ns_user_info_(user_info), is_user_actionable_(is_user_actionable) {
  CHECK(ns_user_info_);
}

IOSDeviceManagementErrorDetails::~IOSDeviceManagementErrorDetails() {}

std::unique_ptr<DeviceManagementErrorDetails>
IOSDeviceManagementErrorDetails::Clone() const {
  return std::make_unique<IOSDeviceManagementErrorDetails>(ns_user_info_,
                                                           is_user_actionable_);
}

bool IOSDeviceManagementErrorDetails::Equals(
    const DeviceManagementErrorDetails& other) const {
  const IOSDeviceManagementErrorDetails* other_ptr =
      static_cast<const IOSDeviceManagementErrorDetails*>(&other);
  return is_user_actionable_ == other_ptr->is_user_actionable_ &&
         [ns_user_info_ isEqualToDictionary:other_ptr->ns_user_info_];
}

bool IOSDeviceManagementErrorDetails::IsUserActionable() const {
  return is_user_actionable_;
}

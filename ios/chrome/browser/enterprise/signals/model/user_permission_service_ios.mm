// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/signals/model/user_permission_service_ios.h"

#import <utility>

#import "components/device_signals/core/browser/user_delegate.h"

UserPermissionServiceIOS::UserPermissionServiceIOS(
    policy::ManagementService* management_service,
    std::unique_ptr<device_signals::UserDelegate> user_delegate,
    PrefService* user_prefs)
    : UserPermissionServiceImpl(management_service,
                                std::move(user_delegate),
                                user_prefs) {}

UserPermissionServiceIOS::~UserPermissionServiceIOS() = default;

bool UserPermissionServiceIOS::ShouldCollectConsent() const {
  // On iOS, there is no separate interactive consent flow for device signals;
  // consent is established when signing into a managed profile.
  return false;
}

bool UserPermissionServiceIOS::HasUserConsented() const {
  return UserPermissionServiceImpl::HasUserConsented() ||
         user_delegate_->IsManagedUser();
}

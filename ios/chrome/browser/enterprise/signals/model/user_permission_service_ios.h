// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_SIGNALS_MODEL_USER_PERMISSION_SERVICE_IOS_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_SIGNALS_MODEL_USER_PERMISSION_SERVICE_IOS_H_

#import <memory>

#import "components/device_signals/core/browser/user_permission_service_impl.h"

class PrefService;

namespace device_signals {
class UserDelegate;
}  // namespace device_signals

namespace policy {
class ManagementService;
}  // namespace policy

// iOS implementation of `device_signals::UserPermissionService`.
//
// On iOS, there is no separate interactive consent flow for device signals;
// consent is established when signing into a managed profile.
class UserPermissionServiceIOS
    : public device_signals::UserPermissionServiceImpl {
 public:
  UserPermissionServiceIOS(
      policy::ManagementService* management_service,
      std::unique_ptr<device_signals::UserDelegate> user_delegate,
      PrefService* user_prefs);

  UserPermissionServiceIOS(const UserPermissionServiceIOS&) = delete;
  UserPermissionServiceIOS& operator=(const UserPermissionServiceIOS&) = delete;

  ~UserPermissionServiceIOS() override;

  // device_signals::UserPermissionService:
  bool ShouldCollectConsent() const override;
  bool HasUserConsented() const override;
};

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_SIGNALS_MODEL_USER_PERMISSION_SERVICE_IOS_H_

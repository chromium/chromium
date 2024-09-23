// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_AUTHENTICATION_SERVICE_OBSERVER_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_AUTHENTICATION_SERVICE_OBSERVER_H_

#include "base/observer_list_types.h"

// Observer handling events related to the AuthenticationService.
class AuthenticationServiceObserver : public base::CheckedObserver {
 public:
  AuthenticationServiceObserver() = default;

  // Called when the primary account is filtered out due to an
  // enterprise restriction.
  virtual void OnPrimaryAccountRestricted() {}

  // Called when the AuthenticationService::GetServiceStatus() value changes.
  // This method might be called with no changes.
  virtual void OnServiceStatusChanged() {}

  // Called when the AuthenticationService::GetPrimaryIdentity() value changes.
  // This method might be called with no changes.
  // TODO(crbug.com/40243387): DEPRECATED: This method should be removed once
  // FakeAuthenticationService is remove. Use IdentityManager notifications.
  virtual void OnPrimaryIdentityChanged() {}
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_AUTHENTICATION_SERVICE_OBSERVER_H_

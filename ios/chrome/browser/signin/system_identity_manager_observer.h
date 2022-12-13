// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_SYSTEM_IDENTITY_MANAGER_OBSERVER_H_
#define IOS_CHROME_BROWSER_SIGNIN_SYSTEM_IDENTITY_MANAGER_OBSERVER_H_

#import <Foundation/Foundation.h>

#include "base/observer_list_types.h"

@protocol RefreshAccessTokenError;
@protocol SystemIdentity;
class SystemIdentityManager;

// Observer handling events related to SystemIdentityManager.
class SystemIdentityManagerObserver : public base::CheckedObserver {
 public:
  SystemIdentityManagerObserver() = default;
  ~SystemIdentityManagerObserver() override = default;

  // Called when the list of identity has changed. If `notify_user` is true,
  // the identity changed due to an external source; this means that a first
  // party Google application has added or removed identies, or the identity
  // token is invalid.
  virtual void OnIdentityListChanged(bool notify_user) {}

  // Called when information about `identity` (such as the name or the image)
  // have been updated.
  virtual void OnIdentityUpdated(id<SystemIdentity> identity) {}

  // Called when refreshing access token for `identity` fails with `error`.
  // The error can be handled by calling `HandleMDMNotification`.
  virtual void OnIdentityAccessTokenRefreshFailed(
      id<SystemIdentity> identity,
      id<RefreshAccessTokenError> error) {}
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_SYSTEM_IDENTITY_MANAGER_OBSERVER_H_

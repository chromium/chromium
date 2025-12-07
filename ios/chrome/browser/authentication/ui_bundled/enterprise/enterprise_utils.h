// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_ENTERPRISE_ENTERPRISE_UTILS_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_ENTERPRISE_ENTERPRISE_UTILS_H_

#import <UIKit/UIKit.h>

#import <optional>

#import "base/functional/callback.h"
#import "ios/chrome/browser/signin/model/system_identity.h"

// Returns YES if some account restrictions are set.
bool IsRestrictAccountsToPatternsEnabled();

// Returns empty if the management status of `identity` is not known, and
// returns true or false when it is known. This is based on the value of hosted
// domain being found in the cache. If the hosted domain is found in cache, call
// `FetchManagedStatusForIdentity` to fetch it and update the cache. The status
// of known consumer identities should always be found immediately by this
// function.
std::optional<BOOL> IsIdentityManaged(id<SystemIdentity> identity);

// Calls `management_status_callback` with a boolean that indicates whether
// `identity` is managed or not. This is based on `identity` having a hosted
// domain. In case of an error while fetching the hosted domain, we assume that
// the account is not managed.
void FetchManagedStatusForIdentity(
    id<SystemIdentity> identity,
    base::OnceCallback<void(BOOL)> management_status_callback);

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_ENTERPRISE_ENTERPRISE_UTILS_H_

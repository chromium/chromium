// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_SYSTEM_IDENTITY_UTIL_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_SYSTEM_IDENTITY_UTIL_H_

#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#include "ios/chrome/browser/signin/model/system_identity.h"

// Returns the primary SystemIdentity for `consent_level`.
id<SystemIdentity> GetPrimarySystemIdentity(
    signin::ConsentLevel consent_level,
    signin::IdentityManager* identity_manager,
    ChromeAccountManagerService* account_manager);

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_SYSTEM_IDENTITY_UTIL_H_

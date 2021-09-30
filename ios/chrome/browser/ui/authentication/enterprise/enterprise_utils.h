// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_ENTERPRISE_ENTERPRISE_UTILS_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_ENTERPRISE_ENTERPRISE_UTILS_H_

#import <UIKit/UIKit.h>

#include "ios/chrome/browser/sync/sync_setup_service.h"

class ChromeBrowserState;

// List of Enterprise restriction options.
typedef NS_OPTIONS(NSUInteger, EnterpriseSignInRestrictions) {
  kNoEnterpriseRestriction = 0,
  kEnterpriseForceSignIn = 1 << 0,
  kEnterpriseRestrictAccounts = 1 << 1,

};

// Returns YES if some account restrictions are set.
bool IsRestrictAccountsToPatternsEnabled();

// Returns true if force signIn is set.
bool IsForceSignInEnabled();

// Returns true if force signIn is set.
bool IsSyncTypesListEnabled();

// Returns true if the |dataType| is managed by policies (i.e. is not syncable).
bool IsManagedSyncDataType(ChromeBrowserState* browserState,
                           SyncSetupService::SyncableDatatype dataType);

// Returns true if any data type is managed by policies (i.e. is not syncable).
bool HasManagedSyncDataType(ChromeBrowserState* browserState);

// Returns current EnterpriseSignInRestrictions.
EnterpriseSignInRestrictions GetEnterpriseSignInRestrictions();

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_ENTERPRISE_ENTERPRISE_UTILS_H_

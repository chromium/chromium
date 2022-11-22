// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_ENTERPRISE_ENTERPRISE_UTILS_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_ENTERPRISE_ENTERPRISE_UTILS_H_

#import <UIKit/UIKit.h>

#include "ios/chrome/browser/sync/sync_setup_service.h"

class AuthenticationService;
class PrefService;
namespace syncer {
class SyncService;
}

// List of Enterprise restriction options.
typedef NS_OPTIONS(NSUInteger, EnterpriseSignInRestrictions) {
  // No enterprise restriction.
  kNoEnterpriseRestriction = 0,
  // Sign-in is forced.
  kEnterpriseForceSignIn = 1 << 0,
  // Sign-in is disabled, please consider using
  // AuthenticationService::GetServiceStatus() to get all disable reasons.
  kEnterpriseSignInDisabled = 1 << 1,
  // Account restrictions are set.
  kEnterpriseRestrictAccounts = 1 << 2,
  // Sync is disabled.
  kEnterpriseSyncDisabled = 1 << 3,
  // If any data type is managed by policies (i.e. is not syncable).
  kEnterpriseSyncTypesListDisabled = 1 << 4,
};

// Returns YES if some account restrictions are set.
bool IsRestrictAccountsToPatternsEnabled();

// Returns true if the `dataType` is managed by policies (i.e. is not syncable).
bool IsManagedSyncDataType(PrefService* pref_service,
                           SyncSetupService::SyncableDatatype dataType);

// Returns true if any data type is managed by policies (i.e. is not syncable).
bool HasManagedSyncDataType(PrefService* pref_service);

// Returns current EnterpriseSignInRestrictions.
EnterpriseSignInRestrictions GetEnterpriseSignInRestrictions(
    AuthenticationService* authentication_service,
    PrefService* pref_service,
    syncer::SyncService* sync_service);

// true if sync is disabled.
bool IsSyncDisabledByPolicy(syncer::SyncService* sync_service);

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_ENTERPRISE_ENTERPRISE_UTILS_H_

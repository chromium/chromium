// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/enterprise/enterprise_utils.h"

#include "base/values.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/driver/sync_service.h"
#include "ios/chrome/browser/application_context.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/policy/policy_util.h"
#include "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Map of all synceable types to the corresponding pref name.
const std::map<SyncSetupService::SyncableDatatype, const char*>
    kSyncableItemTypes = {
        {SyncSetupService::kSyncAutofill, syncer::prefs::kSyncAutofill},
        {SyncSetupService::kSyncBookmarks, syncer::prefs::kSyncBookmarks},
        {SyncSetupService::kSyncOmniboxHistory, syncer::prefs::kSyncTypedUrls},
        {SyncSetupService::kSyncOpenTabs, syncer::prefs::kSyncTabs},
        {SyncSetupService::kSyncPasswords, syncer::prefs::kSyncPasswords},
        {SyncSetupService::kSyncReadingList, syncer::prefs::kSyncReadingList},
        {SyncSetupService::kSyncPreferences, syncer::prefs::kSyncPreferences},
};

}  // namespace

bool IsRestrictAccountsToPatternsEnabled() {
  const base::ListValue* value =
      GetApplicationContext()->GetLocalState()->GetList(
          prefs::kRestrictAccountsToPatterns);
  return !value->GetList().empty();
}

// TODO(crbug.com/1244632): Use the Authentication Service sign-in status API
// instead of this when available.
bool IsForceSignInEnabled() {
  BrowserSigninMode policy_mode = static_cast<BrowserSigninMode>(
      GetApplicationContext()->GetLocalState()->GetInteger(
          prefs::kBrowserSigninPolicy));
  return policy_mode == BrowserSigninMode::kForced;
}

bool IsManagedSyncDataType(ChromeBrowserState* browserState,
                           SyncSetupService::SyncableDatatype dataType) {
  return browserState->GetPrefs()
      ->FindPreference(kSyncableItemTypes.at(dataType))
      ->IsManaged();
}

bool HasManagedSyncDataType(ChromeBrowserState* browserState) {
  for (int type = 0; type != SyncSetupService::kNumberOfSyncableDatatypes;
       type++) {
    SyncSetupService::SyncableDatatype dataType =
        static_cast<SyncSetupService::SyncableDatatype>(type);
    if (IsManagedSyncDataType(browserState, dataType))
      return true;
  }
  return false;
}

EnterpriseSignInRestrictions GetEnterpriseSignInRestrictions(
    ChromeBrowserState* browserState) {
  EnterpriseSignInRestrictions restrictions = kNoEnterpriseRestriction;
  if (IsForceSignInEnabled())
    restrictions |= kEnterpriseForceSignIn;
  if (IsRestrictAccountsToPatternsEnabled())
    restrictions |= kEnterpriseRestrictAccounts;
  if (HasManagedSyncDataType(browserState))
    restrictions |= kEnterpriseSyncTypesListDisabled;
  return restrictions;
}

bool IsSyncDisabledByPolicy(ChromeBrowserState* browserState) {
  syncer::SyncService* syncService =
      SyncServiceFactory::GetForBrowserState(browserState);
  return syncService->GetDisableReasons().Has(
      syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY);
}

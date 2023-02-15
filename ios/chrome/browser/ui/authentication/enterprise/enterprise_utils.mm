// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/enterprise/enterprise_utils.h"

#import "base/containers/fixed_flat_map.h"
#import "base/values.h"
#import "components/policy/policy_constants.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "components/sync/base/pref_names.h"
#import "components/sync/driver/sync_service.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/policy/policy_util.h"
#import "ios/chrome/browser/prefs/pref_names.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Map of all synceable types to the corresponding pref name.
constexpr auto kSyncableItemTypes =
    base::MakeFixedFlatMap<SyncSetupService::SyncableDatatype, const char*>({
        {SyncSetupService::kSyncAutofill, syncer::prefs::kSyncAutofill},
        {SyncSetupService::kSyncBookmarks, syncer::prefs::kSyncBookmarks},
        {SyncSetupService::kSyncOmniboxHistory, syncer::prefs::kSyncTypedUrls},
        {SyncSetupService::kSyncOpenTabs, syncer::prefs::kSyncTabs},
        {SyncSetupService::kSyncPasswords, syncer::prefs::kSyncPasswords},
        {SyncSetupService::kSyncReadingList, syncer::prefs::kSyncReadingList},
        {SyncSetupService::kSyncPreferences, syncer::prefs::kSyncPreferences},
    });

}  // namespace

bool IsRestrictAccountsToPatternsEnabled() {
  return !GetApplicationContext()
              ->GetLocalState()
              ->GetList(prefs::kRestrictAccountsToPatterns)
              .empty();
}

bool IsManagedSyncDataType(PrefService* pref_service,
                           SyncSetupService::SyncableDatatype data_type) {
  return pref_service->FindPreference(kSyncableItemTypes.at(data_type))
      ->IsManaged();
}

bool HasManagedSyncDataType(PrefService* pref_service) {
  for (int type = 0; type != SyncSetupService::kNumberOfSyncableDatatypes;
       type++) {
    SyncSetupService::SyncableDatatype data_type =
        static_cast<SyncSetupService::SyncableDatatype>(type);
    if (IsManagedSyncDataType(pref_service, data_type))
      return true;
  }
  return false;
}

bool IsSyncDisabledByPolicy(syncer::SyncService* sync_service) {
  return sync_service->GetDisableReasons().Has(
      syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY);
}

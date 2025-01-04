// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/history_sync/history_sync_utils.h"

#import "base/time/time.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service.h"
#import "components/sync/base/user_selectable_type.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "ios/chrome/browser/authentication/ui_bundled/history_sync/pref_names.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"

namespace {

// Number of times the History Sync should be successively declined until the
// Opt-In prompt is automatically dismissed.
constexpr int kMaxSuccessiveDeclineCount = 2;

// Delay before reshowing an optional History Sync Opt-In prompt to the user.
constexpr base::TimeDelta kDelayBeforeReshowPrompt = base::Days(14);

}  // namespace

namespace history_sync {

HistorySyncSkipReason GetSkipReason(
    syncer::SyncService* syncService,
    AuthenticationService* authenticationService,
    PrefService* prefService,
    BOOL isOptional) {
  if (syncService->HasDisableReason(
          syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY) ||
      syncService->GetUserSettings()->IsTypeManagedByPolicy(
          syncer::UserSelectableType::kTabs) ||
      syncService->GetUserSettings()->IsTypeManagedByPolicy(
          syncer::UserSelectableType::kHistory)) {
    // Skip History Sync Opt-in if sync is disabled, or if history or
    // tabs sync is disabled by policy.
    return history_sync::HistorySyncSkipReason::kSyncForbiddenByPolicies;
  }
  if (!authenticationService->GetPrimaryIdentity(
          signin::ConsentLevel::kSignin)) {
    // Don't show history sync opt-in screen if no signed-in user account.
    return history_sync::HistorySyncSkipReason::kNotSignedIn;
  }
  syncer::SyncUserSettings* userSettings = syncService->GetUserSettings();
  if (userSettings->GetSelectedTypes().HasAll(
          {syncer::UserSelectableType::kHistory,
           syncer::UserSelectableType::kTabs})) {
    // History opt-in is already set. This value is kept between signout/signin.
    // In this case the UI can be skipped.
    return history_sync::HistorySyncSkipReason::kAlreadyOptedIn;
  }

  if (history_sync::IsDeclinedTooOften(prefService) && isOptional) {
    return history_sync::HistorySyncSkipReason::kDeclinedTooOften;
  }

  return history_sync::HistorySyncSkipReason::kNone;
}

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterTimePref(
      history_sync_prefs::kHistorySyncLastDeclinedTimestamp, base::Time());
  registry->RegisterIntegerPref(
      history_sync_prefs::kHistorySyncSuccessiveDeclineCount, 0);
}

void ResetDeclinePrefs(PrefService* pref_service) {
  pref_service->ClearPref(
      history_sync_prefs::kHistorySyncLastDeclinedTimestamp);
  pref_service->ClearPref(
      history_sync_prefs::kHistorySyncSuccessiveDeclineCount);
}

void RecordDeclinePrefs(PrefService* pref_service) {
  pref_service->SetTime(history_sync_prefs::kHistorySyncLastDeclinedTimestamp,
                        base::Time::Now());
  const int old_count = pref_service->GetInteger(
      history_sync_prefs::kHistorySyncSuccessiveDeclineCount);
  pref_service->SetInteger(
      history_sync_prefs::kHistorySyncSuccessiveDeclineCount, old_count + 1);
}

bool IsDeclinedTooOften(PrefService* pref_service) {
  if (experimental_flags::ShouldIgnoreHistorySyncDeclineLimits()) {
    return false;
  }

  const int decline_count = pref_service->GetInteger(
      history_sync_prefs::kHistorySyncSuccessiveDeclineCount);
  if (decline_count >= kMaxSuccessiveDeclineCount) {
    return true;
  }

  const base::Time last_declined = pref_service->GetTime(
      history_sync_prefs::kHistorySyncLastDeclinedTimestamp);
  return (base::Time::Now() - last_declined) < kDelayBeforeReshowPrompt;
}

}  // namespace history_sync

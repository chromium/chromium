// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/history_sync/history_sync_utils.h"

#import "base/time/time.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/ui/authentication/history_sync/pref_names.h"

namespace {

// Number of times the History Sync should be successively declined until the
// Opt-In prompt is automatically dismissed.
constexpr int kMaxSuccessiveDeclineCount = 2;

// Delay before reshowing an optional History Sync Opt-In prompt to the user.
constexpr base::TimeDelta kDelayBeforeReshowPrompt = base::Days(14);

}  // namespace

namespace history_sync {

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

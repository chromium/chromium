// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_HISTORY_SYNC_HISTORY_SYNC_UTILS_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_HISTORY_SYNC_HISTORY_SYNC_UTILS_H_

class AuthenticationService;
class PrefService;
namespace syncer {
class SyncService;
}
namespace user_prefs {
class PrefRegistrySyncable;
}

namespace history_sync {

// The reasons why the History Sync Opt-In screen should be skipped instead of
// being shown to the user. `kNone` indicates that the screen should not be
// skipped.
enum class HistorySyncSkipReason {
  kNone,
  kNotSignedIn,
  kSyncForbiddenByPolicies,
  kAlreadyOptedIn,
  kDeclinedTooOften,
};

// Checks if the History Sync Opt-In screen should be skipped, and returns the
// corresponding reason. history_sync::HistorySyncSkipReason::kNone means that
// the screen should not be skipped.
HistorySyncSkipReason GetSkipReason(
    syncer::SyncService* syncService,
    AuthenticationService* authenticationService,
    PrefService* prefService,
    bool isOptional);

// Registers prefs used to skip too frequent History Sync Opt-In prompt.
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

// Resets prefs related to declined History Sync Opt-In prompt.
void ResetDeclinePrefs(PrefService* pref_service);

// Records that History Sync has been declined in prefs.
void RecordDeclinePrefs(PrefService* pref_service);

// Whether the History Sync was declined too often, and that the Opt-In screen
// should be skipped because of this, if it is optional.
bool IsDeclinedTooOften(PrefService* pref_service);

}  // namespace history_sync

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_HISTORY_SYNC_HISTORY_SYNC_UTILS_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_HISTORY_SYNC_HISTORY_SYNC_UTILS_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_HISTORY_SYNC_HISTORY_SYNC_UTILS_H_

class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace history_sync {

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

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_HISTORY_SYNC_HISTORY_SYNC_UTILS_H_

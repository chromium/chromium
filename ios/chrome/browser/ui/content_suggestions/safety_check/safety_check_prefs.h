// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SAFETY_CHECK_SAFETY_CHECK_PREFS_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SAFETY_CHECK_SAFETY_CHECK_PREFS_H_

class PrefRegistrySimple;
class PrefService;

namespace safety_check_prefs {

// Pref name that disables Safety Check.
extern const char kSafetyCheckInMagicStackDisabledPref[];

// Registers the prefs associated with Safety Check.
void RegisterPrefs(PrefRegistrySimple* registry);

// Returns `true` if Safety Check has been disabled by the user.
bool IsSafetyCheckInMagicStackDisabled(PrefService* prefs);

// Disables Safety Check.
void DisableSafetyCheckInMagicStack(PrefService* prefs);

}  // namespace safety_check_prefs

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SAFETY_CHECK_SAFETY_CHECK_PREFS_H_

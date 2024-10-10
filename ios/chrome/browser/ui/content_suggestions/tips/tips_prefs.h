// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_TIPS_TIPS_PREFS_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_TIPS_TIPS_PREFS_H_

class PrefRegistrySimple;
class PrefService;

namespace tips_prefs {

// Pref that indicates whether the Tips module is disabled.
extern const char kTipsInMagicStackDisabledPref[];

// Registers the prefs associated with the Tips module.
void RegisterPrefs(PrefRegistrySimple* registry);

// Returns `true` if the Tips module is disabled.
bool IsTipsInMagicStackDisabled(PrefService* prefs);

// Disables the Tips module.
void DisableTipsInMagicStack(PrefService* prefs);

}  // namespace tips_prefs

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_TIPS_TIPS_PREFS_H_

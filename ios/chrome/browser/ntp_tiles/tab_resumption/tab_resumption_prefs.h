// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_TILES_TAB_RESUMPTION_TAB_RESUMPTION_PREFS_H_
#define IOS_CHROME_BROWSER_NTP_TILES_TAB_RESUMPTION_TAB_RESUMPTION_PREFS_H_

class PrefRegistrySimple;
class PrefService;

namespace tab_resumption_prefs {

// Pref name that disable the tab resumption tile.
extern const char kTabResumptioDisabledPref[];

// Registers the prefs associated with the tab resumption tile.
void RegisterPrefs(PrefRegistrySimple* registry);

// Returns `true` if the tab resumption tile has been disabled.
bool IsTabResumptionDisabled(PrefService* prefs);

// Disables the tab resumption tile.
void DisableTabResumption(PrefService* prefs);

}  // namespace tab_resumption_prefs

#endif  // IOS_CHROME_BROWSER_NTP_TILES_TAB_RESUMPTION_TAB_RESUMPTION_PREFS_H_

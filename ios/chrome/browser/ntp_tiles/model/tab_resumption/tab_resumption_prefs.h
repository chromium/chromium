// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_TILES_MODEL_TAB_RESUMPTION_TAB_RESUMPTION_PREFS_H_
#define IOS_CHROME_BROWSER_NTP_TILES_MODEL_TAB_RESUMPTION_TAB_RESUMPTION_PREFS_H_

class GURL;
class PrefRegistrySimple;
class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace tab_resumption_prefs {

// Pref name that disables the tab resumption tile.
extern const char kTabResumptioDisabledPref[];

// Pref name that stores the last opened tab URL.
extern const char kTabResumptionLastOpenedTabURLPref[];

// Registers the local state prefs associated with the tab resumption tile.
void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

// Registers the profile prefs associated with the tab resumption tile.
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

// Returns `true` if the tab resumption tile has been disabled.
bool IsTabResumptionDisabled(PrefService* prefs);

// Disables the tab resumption tile.
void DisableTabResumption(PrefService* prefs);

// Returns `true` if the given `URL` matches the last opened tab URL.
bool IsLastOpenedURL(GURL URL, PrefService* prefs);

// Sets the last opened tab URL.
void SetTabResumptionLastOpenedTabURL(GURL URL, PrefService* prefs);

}  // namespace tab_resumption_prefs

#endif  // IOS_CHROME_BROWSER_NTP_TILES_MODEL_TAB_RESUMPTION_TAB_RESUMPTION_PREFS_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_BROWSER_STATE_PREFS_H_
#define IOS_WEB_VIEW_INTERNAL_BROWSER_STATE_PREFS_H_

class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace ios_web_view {

// Registers the BrowserState preferences for this `pref_registry`.
void RegisterBrowserStatePrefs(user_prefs::PrefRegistrySyncable* pref_registry);

// Clears or migrates obsolete BrowserState preferences in `prefs`. Must be
// called once after `prefs` is created.
void MigrateObsoleteBrowserStatePrefs(PrefService* prefs);

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_BROWSER_STATE_PREFS_H_

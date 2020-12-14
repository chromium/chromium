// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PREFS_PREFS_UTIL_H_
#define IOS_CHROME_BROWSER_PREFS_PREFS_UTIL_H_

class PrefService;

// Returns true if incognito mode is disabled by enterprise policy.
bool IsIncognitoModeDisabled(PrefService* pref_service);

// Returns true if incognito mode is forced by enterprise policy.
bool IsIncognitoModeForced(PrefService* pref_service);

#endif  // IOS_CHROME_BROWSER_PREFS_PREFS_UTIL_H_

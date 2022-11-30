// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_POLICY_UTIL_H_
#define IOS_CHROME_BROWSER_POLICY_POLICY_UTIL_H_

class PrefService;

// Values for the BrowserSignin policy.
// VALUES MUST COINCIDE WITH THE BrowserSignin POLICY DEFINITION.
enum class BrowserSigninMode {
  kDisabled = 0,
  kEnabled = 1,
  kForced = 2,
};

// The enum class for IncognitoModeAvalibility pref value, explains the meaning
// of each value.
enum class IncognitoModePrefs {
  // Incognito mode enabled. Users may open pages in both Incognito mode and
  // normal mode (usually the default behaviour).
  kEnabled = 0,
  // Incognito mode disabled. Users may not open pages in Incognito mode.
  // Only normal mode is available for browsing.
  kDisabled,
  // Incognito mode forced. Users may open pages *ONLY* in Incognito mode.
  // Normal mode is not available for browsing.
  kForced,
};

// Returns true if IncognitoModeAvailability policy is set by enterprise.
bool IsIncognitoPolicyApplied(PrefService* pref_service);

// Returns true if incognito mode is disabled by enterprise policy.
bool IsIncognitoModeDisabled(PrefService* pref_service);

// Returns true if incognito mode is forced by enterprise policy.
bool IsIncognitoModeForced(PrefService* pref_service);

// Returns whether the browser is managed based on the presence of policy data
// in the App Configuration from the platform.
bool IsApplicationManagedByPlatform();

#endif  // IOS_CHROME_BROWSER_POLICY_POLICY_UTIL_H_

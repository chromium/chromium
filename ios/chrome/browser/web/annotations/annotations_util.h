// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_ANNOTATIONS_ANNOTATIONS_UTIL_H_
#define IOS_CHROME_BROWSER_WEB_ANNOTATIONS_ANNOTATIONS_UTIL_H_

class PrefService;

// Returns whether the address detection feature is enabled.
bool IsAddressDetectionEnabled();

// Returns whether the automatic detection settings is enabled.
// Note: If one-tap-address_detection is disabled, the setting
// is not present and default to true.
bool IsAddressAutomaticDetectionEnabled(PrefService* prefs);

// Whether the user accepted the address detection one tap interstitial.
bool IsAddressAutomaticDetectionAccepted(PrefService* prefs);

// Whether the consent screen should be presented to the user.
bool ShouldPresentConsentScreen(PrefService* prefs);

// Whether the IPH screen for consent should be presented to the user.
bool ShouldPresentConsentIPH(PrefService* prefs);

// Returns whether the long press detection is enabled.
// Note: If one-tap-address is disabled, the setting
// is not present and default to true.
bool IsAddressLongPressDetectionEnabled(PrefService* prefs);

#endif  // IOS_CHROME_BROWSER_WEB_ANNOTATIONS_ANNOTATIONS_UTIL_H_

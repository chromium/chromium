// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INCOGNITO_INTERSTITIAL_UI_BUNDLED_INCOGNITO_INTERSTITIAL_CONSTANTS_H_
#define IOS_CHROME_BROWSER_INCOGNITO_INTERSTITIAL_UI_BUNDLED_INCOGNITO_INTERSTITIAL_CONSTANTS_H_

@class NSString;

// The accessibility identifier for the Incognito interstitial.
extern NSString* const kIncognitoInterstitialAccessibilityIdentifier;

// The accessibility identifier for the Incognito interstitial URL label.
extern NSString* const kIncognitoInterstitialURLLabelAccessibilityIdentifier;

// The accessibility identifier for the Cancel button in the Incognito
// interstitial.
extern NSString* const
    kIncognitoInterstitialCancelButtonAccessibilityIdentifier;

// UMA histogram names.
extern const char kIncognitoInterstitialActionsHistogram[];
extern const char kIncognitoInterstitialSettingsActionsHistogram[];

// Enum for the IOS.IncognitoInterstitial histogram.
// Keep in sync with "IncognitoInterstitialActionType"
// in src/tools/metrics/histograms/enums.xml.
enum class IncognitoInterstitialActions {
  kUnknown = 0,  // Never logged.
  kOpenInChromeIncognito = 1,
  // The user chose to open link in a "normal" tab.
  kOpenInChrome = 2,
  // The user triggered the "Cancel" button.
  kCancel = 3,
  // The user tapped the "Learn more about Incognito" link.
  kLearnMore = 4,
  // The Incognito interstitial was dismissed by an external event.
  kExternalDismissed = 5,
  kMaxValue = kExternalDismissed,
};

// Enum for the IOS.IncognitoInterstitial.Settings histogram.
// Keep in sync with "IncognitoInterstitialSettingsActionType"
// in src/tools/metrics/histograms/enums.xml.
enum class IncognitoInterstitialSettingsActions {
  kUnknown = 0,  // Never logged.
  kEnabled = 1,
  kDisabled = 2,
  kMaxValue = kDisabled,
};

#endif  // IOS_CHROME_BROWSER_INCOGNITO_INTERSTITIAL_UI_BUNDLED_INCOGNITO_INTERSTITIAL_CONSTANTS_H_

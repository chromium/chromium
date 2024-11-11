// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INCOGNITO_REAUTH_UI_BUNDLED_INCOGNITO_REAUTH_CONSTANTS_H_
#define IOS_CHROME_BROWSER_INCOGNITO_REAUTH_UI_BUNDLED_INCOGNITO_REAUTH_CONSTANTS_H_

#import "base/time/time.h"

// Enum that captures the type of overlay, if any, that is displayed over
// incognito content.
enum class IncognitoLockState {
  // No overlay should be displayed over the incognito content.
  kNone,
  // An overlay is displayed over incognito content that requires
  // reauthentication in order to dismiss.
  kReauth,
  // An overlay is displayed over incognito content that requires a tap in order
  // to dismiss.
  kSoftLock,
};

// Histogram name for Incognito lock setting interactions.
const char kIncognitoLockSettingInteractionHistogram[] =
    "IOS.IncognitoLockSettingInteraction";

// Enum for the IOS.IncognitoLockSettingInteraction histogram.
// LINT.IfChange(IncognitoLockSettingInteraction)
enum class IncognitoLockSettingInteraction {
  // User selected Don't hide option in settings.
  kDoNotHideSelected = 0,
  // User selected hide with reauth method option in settings.
  kHideWithReauthSelected,
  // User selected wide with soft lock option in settings
  kHideWithSoftLockSelected,
  kMaxValue = kHideWithSoftLockSelected,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ios/enums.xml:IncognitoLockSettingInteractionType)

// Histogram name for Incognito lock overlay interactions.
const char kIncognitoLockOverlayInteractionHistogram[] =
    "IOS.IncognitoLockOverlayInteraction";

// Enum for the IOS.IncognitoLockOverlayInteraction histogram.
// LINT.IfChange(IncognitoLockOverlayInteraction)
enum class IncognitoLockOverlayInteraction {
  // User clicked unlock with reauth method button on the overlay.
  kUnlockWithReauthButtonClicked = 0,
  // User clicked continue in incognito button on the overlay.
  kContinueInIncognitoButtonClicked,
  // User clicked close incognito tabs button on the overlay.
  kCloseIncognitoTabsButtonClicked,
  // User clicked see other tabs button on the overlay.
  kSeeOtherTabsButtonClicked,
  kMaxValue = kSeeOtherTabsButtonClicked,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ios/enums.xml:IncognitoLockOverlayInteractionType)

#endif  // IOS_CHROME_BROWSER_INCOGNITO_REAUTH_UI_BUNDLED_INCOGNITO_REAUTH_CONSTANTS_H_

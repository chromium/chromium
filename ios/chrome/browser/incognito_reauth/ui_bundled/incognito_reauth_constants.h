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

// Enum for the IOS.IncognitoLockSettingInteraction histogram. These values are
// persisted to logs. Entries should not be renumbered and numeric values should
// never be reused.
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

// Enum for the IOS.IncognitoLockOverlayInteraction histogram.These values are
// persisted to logs. Entries should not be renumbered and numeric values should
// never be reused.
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

// Histogram name for Incognito lock overlay interactions.
const char kIncognitoLockImpressionHistogram[] = "IOS.IncognitoLockImpression";

// Enum for the IOS.IncognitoLockImpression histogram. These values are
// persisted to logs. Entries should not be renumbered and numeric values should
// never be reused.
// LINT.IfChange(IncognitoLockImpression)
enum class IncognitoLockImpression {
  // User saw Incognito soft lock on a single tab.
  kSoftLockSingleTab = 0,
  // User saw Incognito soft lock on the tab grid.
  kSoftLockTabGrid,
  // User saw Incognito reauth lock on a single tab.
  kReauthLockSingleTab,
  // User saw Incognito reauth lock on tab grid.
  kReauthLockTabGrid,
  kMaxValue = kReauthLockTabGrid,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ios/enums.xml:IncognitoLockImpressionType)

// Histogram name for Incognito lock setting state on startup.
const char kIncognitoLockSettingStartupStateHistogram[] =
    "IOS.IncognitoLockSettingStartupState";

// Enum that captures the type Incognito lock state on startup. These values are
// persisted to logs. Entries should not be renumbered and numeric values should
// never be reused.
// LINT.IfChange(IncognitoLockSettingStartupState)
enum class IncognitoLockSettingStartupState {
  // Lock setting is set to do not hide.
  kDoNotHide = 0,
  // Lock setting is set to hide with reauthentication method.
  kHideWithReauth,
  // Lock setting is set to hide with soft lock.
  kHideWithSoftLock,
  kMaxValue = kHideWithSoftLock,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ios/enums.xml:IncognitoLockSettingStartupState)

#endif  // IOS_CHROME_BROWSER_INCOGNITO_REAUTH_UI_BUNDLED_INCOGNITO_REAUTH_CONSTANTS_H_

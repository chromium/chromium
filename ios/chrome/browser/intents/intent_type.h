// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTENTS_INTENT_TYPE_H_
#define IOS_CHROME_BROWSER_INTENTS_INTENT_TYPE_H_

// All intent types available for donation.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Logged as IOSSpotlightDonatedIntentType enum for
// IOS.Spotlight.DonatedIntentType histogram.
enum class IntentType {
  kSearchInChrome = 0,         // SearchInChromeIntent
  kOpenReadingList = 1,        // OpenReadingListIntent
  kOpenBookmarks = 2,          // OpenBookmarksIntent
  kOpenRecentTabs = 3,         // OpenRecentTabsIntent
  kOpenTabGrid = 4,            // OpenTabGridIntent
  kOpenVoiceSearch = 5,        // SearchWithVoiceIntent
  kOpenNewTab = 6,             // OpenNewTabIntent
  kPlayDinoGame = 7,           // PlayDinoGameIntent
  kSetDefaultBrowser = 8,      // SetChromeDefaultBrowserIntent
  kViewHistory = 9,            // ViewHistoryIntent
  kOpenLatestTab = 10,         // OpenLatestTabIntent
  kStartLens = 11,             // OpenLensIntent
  kClearBrowsingData = 12,     // ClearBrowsingDataIntent
  kOpenInChrome = 13,          // OpenInChromeIntent
  kOpenInIncognito = 14,       // OpenInIncognitoIntent
  kOpenNewIncognitoTab = 15,   // OpenNewIncognitoTabIntent
  kManagePaymentMethods = 16,  // ManagePaymentMethodsIntent
  kRunSafetyCheck = 17,        // RunSafetyCheckIntent
  kManagePasswords = 18,       // ManagePasswordsIntent
  kManageSettings = 19,        // ManageSettingsIntent
  kMaxValue = kManageSettings,
};
// LINT.ThenChange(src/tools/metrics/histograms/enums.xml:IOSSpotlightDonatedIntentType)

#endif  // IOS_CHROME_BROWSER_INTENTS_INTENT_TYPE_H_

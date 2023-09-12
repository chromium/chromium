// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTENTS_INTENTS_DONATION_HELPER_H_
#define IOS_CHROME_BROWSER_INTENTS_INTENTS_DONATION_HELPER_H_

#import <Foundation/Foundation.h>

// All intent types available for donation.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Logged as IOSSpotlightDonatedIntentType enum for
// IOS.Spotlight.DonatedIntentType histogram.
enum class DonatedIntentType {
  kSearchInChrome = 0,      // SearchInChromeIntent
  kOpenReadingList = 1,     // OpenReadingListIntent
  kOpenBookmarks = 2,       // OpenBookmarksIntent
  kOpenRecentTabs = 3,      // OpenRecentTabsIntent
  kOpenTabGrid = 4,         // OpenTabGridIntent
  kOpenVoiceSearch = 5,     // SearchWithVoiceIntent
  kOpenNewTab = 6,          // OpenNewTabIntent
  kPlayDinoGame = 7,        // PlayDinoGameIntent
  kSetDefaultBrowser = 8,   // SetChromeDefaultBrowserIntent
  kViewHistory = 9,         // ViewHistoryIntent
  kOpenLatestTab = 10,      // OpenLatestTabIntent
  kStartLens = 11,          // OpenLensIntent
  kClearBrowsingData = 12,  // ClearBrowsingDataIntent
  kMaxValue = kClearBrowsingData,
};

/// Set of utils for donating INInteractions matching INIntents.
@interface IntentDonationHelper : NSObject

/// Donate the intent of given type to IntentKit.
+ (void)donateIntent:(DonatedIntentType)intentType;

@end

#endif  // IOS_CHROME_BROWSER_INTENTS_INTENTS_DONATION_HELPER_H_

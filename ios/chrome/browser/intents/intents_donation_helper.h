// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTENTS_INTENTS_DONATION_HELPER_H_
#define IOS_CHROME_BROWSER_INTENTS_INTENTS_DONATION_HELPER_H_

#import <Foundation/Foundation.h>

// All intent types available for donation.
enum IntentType {
  INTENT_SEARCH_IN_CHROME,   // SearchInChromeIntent
  INTENT_OPEN_READING_LIST,  // OpenReadingListIntent
  INTENT_OPEN_BOOKMARKS,     // OpenBookmarksIntent
  INTENT_TYPE_COUNT,
};

/// Set of utils for donating INInteractions matching INIntents.
@interface IntentDonationHelper : NSObject

/// Donate the intent of given type to IntentKit.
+ (void)donateIntent:(IntentType)intentType;

@end

#endif  // IOS_CHROME_BROWSER_INTENTS_INTENTS_DONATION_HELPER_H_

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_APP_STORE_RATING_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_APP_STORE_RATING_CONSTANTS_H_

#import <Foundation/Foundation.h>

// Key used to store the total number of unique days that the user has
// started a session in NSUserDefaults.
extern NSString* const kAppStoreRatingTotalDaysOnChromeKey;

// Key used to store an array of unique days that the user has started
// a session in the past 7 days in NSUserDefaults.
extern NSString* const kAppStoreRatingActiveDaysInPastWeekKey;

// Key used to store the latest date the App Store Rating promo was
// requested for display for the user.
extern NSString* const kAppStoreRatingLastShownPromoDayKey;

#endif  // IOS_CHROME_BROWSER_UI_APP_STORE_RATING_CONSTANTS_H_

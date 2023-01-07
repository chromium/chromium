// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_CLEAR_BROWSING_DATA_UI_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_CLEAR_BROWSING_DATA_UI_CONSTANTS_H_

#import <Foundation/Foundation.h>

extern NSString* const kClearBrowsingDataButtonIdentifier;

/* UMA histogram values for My Activity Navigiation.
 * Note: this should stay in sync with
 * ClearBrowsingDataMyActivityNavigation in enums.xml. */
enum class MyActivityNavigation {
  kTopLevel = 0,
  kSearchHistory = 1,
  // New elements go above.
  kMaxValue = kSearchHistory,
};

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_CLEAR_BROWSING_DATA_UI_CONSTANTS_H_

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_CONFIGURING_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_CONFIGURING_H_

#include "ios/chrome/browser/discover_feed/model/feed_constants.h"

// Protocol containing the properties to configure the NTP.
@protocol NewTabPageConfiguring

// Whether the NTP should initially be scrolled into the feed.
@property(nonatomic, assign) BOOL shouldScrollIntoFeed;

// Changes the selected feed on the NTP to be `feedType`.
- (void)selectFeedType:(FeedType)feedType;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_CONFIGURING_H_

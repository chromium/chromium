// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_CONFIGURING_H_
#define IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_CONFIGURING_H_

#include "ios/chrome/browser/discover_feed/feed_constants.h"

// Protocol containing the properties to configure the NTP.
@protocol NewTabPageConfiguring

// Currently selected feed.
@property(nonatomic, assign) FeedType selectedFeed;

// Whether the NTP should initially be scrolled into the feed.
@property(nonatomic, assign) BOOL shouldScrollIntoFeed;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_CONFIGURING_H_

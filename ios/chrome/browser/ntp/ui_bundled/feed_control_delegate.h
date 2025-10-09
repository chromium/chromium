// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_FEED_CONTROL_DELEGATE_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_FEED_CONTROL_DELEGATE_H_

#import "ios/chrome/browser/discover_feed/model/feed_constants.h"

// Delegate for controlling the presented feed.
@protocol FeedControlDelegate

// Returns the index of the last visible feed card.
- (NSUInteger)lastVisibleFeedCardIndex;

// Updates the feed header when the default search engine changes.
- (void)updateFeedForDefaultSearchEngineChanged;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_FEED_CONTROL_DELEGATE_H_

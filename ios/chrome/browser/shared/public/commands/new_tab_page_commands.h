// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_NEW_TAB_PAGE_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_NEW_TAB_PAGE_COMMANDS_H_

#import <UIKit/UIKit.h>

#include "ios/chrome/browser/discover_feed/model/feed_constants.h"

typedef NS_ENUM(NSInteger, FeedLayoutUpdateType);

// Commands related to the new tab page.
@protocol NewTabPageCommands

// Notifies the feed model on the new tab page has completed updates.
// This can include, initial loading of cards, pagination, card removal, and
// refreshes.
- (void)handleFeedModelDidEndUpdates:(FeedLayoutUpdateType)updateType;

// Presents an IPH bubble to highlight the Lens icon in the NTP Fakebox.
- (void)presentLensIconBubble;

// Presents an IPH bubble to highlight scrolling on the feed.
- (void)presentFeedSwipeFirstRunBubble;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_NEW_TAB_PAGE_COMMANDS_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_DISCOVER_FEED_MANAGE_DELEGATE_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_DISCOVER_FEED_MANAGE_DELEGATE_H_

#import <UIKit/UIKit.h>

// Protocol for actions relating to the Discover feed management.
@protocol DiscoverFeedManageDelegate

// Handles the action when users tap on manage the discover feed.
- (void)didTapDiscoverFeedManage;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_DISCOVER_FEED_MANAGE_DELEGATE_H_

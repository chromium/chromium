// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_FEED_MANAGEMENT_FEED_MANAGEMENT_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_NTP_FEED_MANAGEMENT_FEED_MANAGEMENT_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller.h"

@protocol FeedManagementFollowDelegate;
@protocol FeedManagementNavigationDelegate;

// The UI that displays various settings for the feed (e.g., following,
// interests, hidden, activity).
@interface FeedManagementViewController : ChromeTableViewController

// Delegate to execute user actions related to follow management.
@property(nonatomic, weak) id<FeedManagementFollowDelegate> followDelegate;

// Delegate to execute user actions related to navigation.
@property(nonatomic, weak) id<FeedManagementNavigationDelegate>
    navigationDelegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_FEED_MANAGEMENT_FEED_MANAGEMENT_VIEW_CONTROLLER_H_

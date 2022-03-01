// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_FEED_MANAGEMENT_FEED_MANAGEMENT_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_NTP_FEED_MANAGEMENT_FEED_MANAGEMENT_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/ntp/feed_management/feed_management_delegate.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller.h"

// The UI that displays various settings for the feed (e.g., following,
// interests, hidden, activity).
@interface FeedManagementViewController : ChromeTableViewController

// Delegate to execute user actions originating from this UI.
@property(nonatomic, weak) id<FeedManagementDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_FEED_MANAGEMENT_FEED_MANAGEMENT_VIEW_CONTROLLER_H_

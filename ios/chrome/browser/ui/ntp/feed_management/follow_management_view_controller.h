// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_FEED_MANAGEMENT_FOLLOW_MANAGEMENT_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_NTP_FEED_MANAGEMENT_FOLLOW_MANAGEMENT_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/ntp/feed_management/follow_management_delegate.h"
#import "ios/chrome/browser/ui/ntp/feed_management/followed_web_channels_data_source.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller.h"

// The UI that displays the web channels that the user is following.
@interface FollowManagementViewController : ChromeTableViewController

// DataSource for followed web channels.
@property(nonatomic, weak) id<FollowedWebChannelsDataSource> dataSource;

// Delegate for meaningful user actions.
@property(nonatomic, weak) id<FollowManagementDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_FEED_MANAGEMENT_FOLLOW_MANAGEMENT_VIEW_CONTROLLER_H_

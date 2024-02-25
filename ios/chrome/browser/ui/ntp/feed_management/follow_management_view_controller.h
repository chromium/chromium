// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_FEED_MANAGEMENT_FOLLOW_MANAGEMENT_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_NTP_FEED_MANAGEMENT_FOLLOW_MANAGEMENT_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller.h"
#import "ios/chrome/browser/ui/ntp/feed_management/follow_management_ui_updater.h"

@protocol FeedManagementNavigationDelegate;
@class FeedMetricsRecorder;
@protocol FollowedWebChannelsDataSource;
@protocol FollowManagementViewDelegate;
@protocol TableViewFaviconDataSource;
@protocol FollowManagementFollowDelegate;

// The UI that displays the web channels that the user is following.
@interface FollowManagementViewController
    : LegacyChromeTableViewController <FollowManagementUIUpdater>

// Delegate for view events.
@property(nonatomic, weak) id<FollowManagementViewDelegate> viewDelegate;

// DataSource for followed web channels.
@property(nonatomic, weak) id<FollowedWebChannelsDataSource>
    followedWebChannelsDataSource;

// Source for favicons.
@property(nonatomic, weak) id<TableViewFaviconDataSource> faviconDataSource;

// Feed metrics recorder.
@property(nonatomic, weak) FeedMetricsRecorder* feedMetricsRecorder;

// Delegate to execute user actions related to navigation.
@property(nonatomic, weak) id<FeedManagementNavigationDelegate>
    navigationDelegate;

// Delegate to unfollow a channel.
@property(nonatomic, weak) id<FollowManagementFollowDelegate> followDelegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_FEED_MANAGEMENT_FOLLOW_MANAGEMENT_VIEW_CONTROLLER_H_

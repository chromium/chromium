// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_FEED_MANAGEMENT_FOLLOW_MANAGEMENT_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_NTP_FEED_MANAGEMENT_FOLLOW_MANAGEMENT_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/ntp/feed_management/follow_management_follow_delegate.h"
#import "ios/chrome/browser/ui/ntp/feed_management/followed_web_channels_data_source.h"
#import "ios/chrome/browser/ui/table_view/table_view_favicon_data_source.h"

class Browser;
@protocol FollowManagementUIUpdater;

// The intermediary between the model and view layers for the follow management
// UI.
@interface FollowManagementMediator : NSObject <FollowedWebChannelsDataSource,
                                                TableViewFaviconDataSource,
                                                FollowManagementFollowDelegate>

// Init method. `browser` can't be nil.
- (instancetype)initWithBrowser:(Browser*)browser NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Add the FollowManagementUIUpdater `updater`.
- (void)addFollowManagementUIUpdater:(id<FollowManagementUIUpdater>)updater;

// Remove he FollowManagementUIUpdater `updater`.
- (void)removeFollowManagementUIUpdater:(id<FollowManagementUIUpdater>)updater;

// Detach the mediator. Must be called before -dealloc.
- (void)detach;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_FEED_MANAGEMENT_FOLLOW_MANAGEMENT_MEDIATOR_H_

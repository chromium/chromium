// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_FEED_MANAGEMENT_FOLLOWED_WEB_CHANNELS_DATA_SOURCE_H_
#define IOS_CHROME_BROWSER_UI_NTP_FEED_MANAGEMENT_FOLLOWED_WEB_CHANNELS_DATA_SOURCE_H_

#import <Foundation/Foundation.h>

@class FollowedWebChannel;

// A data source from which the UI can pull a list of followed web channels.
@protocol FollowedWebChannelsDataSource

// Returns an array of WebChannels. This must be synchronous.
@property(nonatomic, readonly)
    NSArray<FollowedWebChannel*>* followedWebChannels;

// Loads followed websites.
- (void)loadFollowedWebSites;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_FEED_MANAGEMENT_FOLLOWED_WEB_CHANNELS_DATA_SOURCE_H_

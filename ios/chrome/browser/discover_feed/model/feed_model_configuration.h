// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DISCOVER_FEED_MODEL_FEED_MODEL_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_DISCOVER_FEED_MODEL_FEED_MODEL_CONFIGURATION_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/discover_feed/model/feed_constants.h"

// Configuration object for creating feed models.
@interface FeedModelConfiguration : NSObject

- (instancetype)init NS_UNAVAILABLE;

// Creates the configuration for a Discover feed.
+ (instancetype)discoverFeedModelConfiguration;

// Creates the configuration for a Following feed with a given `sortType`.
+ (instancetype)followingModelConfigurationWithSortType:
    (FollowingFeedSortType)sortType;

// The type of feed to be created.
@property(nonatomic, readonly) FeedType feedType;

// The sorting order for the Following feed. Only used if `feedType` is
// the Following feed. Otherwise, returns `FollowingFeedSortTypeUndefined`.
@property(nonatomic, readonly) FollowingFeedSortType followingFeedSortType;

@end

#endif  // IOS_CHROME_BROWSER_DISCOVER_FEED_MODEL_FEED_MODEL_CONFIGURATION_H_

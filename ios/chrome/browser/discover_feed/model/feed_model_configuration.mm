// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/discover_feed/model/feed_model_configuration.h"

#import "base/check_op.h"

@implementation FeedModelConfiguration

#pragma mark - Public

+ (instancetype)discoverFeedModelConfiguration {
  return [[self alloc] initWithFeedType:FeedTypeDiscover
                  followingFeedSortType:FollowingFeedSortTypeUnspecified];
}

+ (instancetype)followingModelConfigurationWithSortType:
    (FollowingFeedSortType)sortType {
  DCHECK_NE(sortType, FollowingFeedSortTypeUnspecified);
  return [[self alloc] initWithFeedType:FeedTypeFollowing
                  followingFeedSortType:sortType];
}

#pragma mark - Private

// Initializes `self` with a `feedType` and a `sortType`.
- (instancetype)initWithFeedType:(FeedType)feedType
           followingFeedSortType:(FollowingFeedSortType)sortType {
  if ((self = [super init])) {
    _feedType = feedType;
    _followingFeedSortType = sortType;
  }
  return self;
}

@end

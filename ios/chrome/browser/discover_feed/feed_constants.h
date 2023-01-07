// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DISCOVER_FEED_FEED_CONSTANTS_H_
#define IOS_CHROME_BROWSER_DISCOVER_FEED_FEED_CONSTANTS_H_

#import <Foundation/Foundation.h>

// Enum representing the different feeds displayed on the NTP.
typedef NS_ENUM(NSInteger, FeedType) {
  FeedTypeDiscover = 0,
  FeedTypeFollowing
};

// The types of sorting for the Following feed.
typedef NS_ENUM(NSInteger, FollowingFeedSortType) {
  // Does not provide a sort type. Used for non-Following feeds.
  FollowingFeedSortTypeUnspecified = 0,
  // Sorts content in publisher groups that can be expanded.
  FollowingFeedSortTypeByPublisher,
  // Sorts content in reverse-chronological order without groups.
  FollowingFeedSortTypeByLatest
};

// The identifier used to register and schedule background feed refresh tasks.
extern NSString* const kFeedBackgroundRefreshTaskIdentifier;

// The user defaults key indicating if the user has ever engaged with a feed.
extern NSString* const kEngagedWithFeedKey;

#endif  // IOS_CHROME_BROWSER_DISCOVER_FEED_FEED_CONSTANTS_H_

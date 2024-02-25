// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DISCOVER_FEED_MODEL_DISCOVER_FEED_REFRESHER_H_
#define IOS_CHROME_BROWSER_DISCOVER_FEED_MODEL_DISCOVER_FEED_REFRESHER_H_

enum class FeedRefreshTrigger;
@class NSDate;

// An interface to refresh the Discover Feed.
class DiscoverFeedRefresher {
 public:
  // Refreshes the Discover Feed. `trigger` describes the context of the
  // refresh.
  virtual void RefreshFeed(FeedRefreshTrigger trigger) = 0;

  // Performs a background refresh for the feed. `completion` is called
  // after success, failure, or timeout. The BOOL argument indicates whether the
  // refresh was successful or a failure.
  virtual void PerformBackgroundRefreshes(void (^completion)(BOOL)) = 0;

  // Stops the background refresh task and cleans up any temporary objects. This
  // is called by the OS when the task is taking too long.
  virtual void HandleBackgroundRefreshTaskExpiration() = 0;

  // The earliest datetime at which the next background refresh should be
  // scheduled.
  virtual NSDate* GetEarliestBackgroundRefreshBeginDate() = 0;
};

#endif  // IOS_CHROME_BROWSER_DISCOVER_FEED_MODEL_DISCOVER_FEED_REFRESHER_H_

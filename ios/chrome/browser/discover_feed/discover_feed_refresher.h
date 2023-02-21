// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DISCOVER_FEED_DISCOVER_FEED_REFRESHER_H_
#define IOS_CHROME_BROWSER_DISCOVER_FEED_DISCOVER_FEED_REFRESHER_H_

// An interface to refresh the Discover Feed.
class DiscoverFeedRefresher {
 public:
  // Refreshes the Discover Feed, indicating whether the feed is visible at the
  // time of the request.
  virtual void RefreshFeed(bool feed_visible) = 0;

  // Refreshes the Discover Feed if needed. The implementer decides if a refresh
  // is needed or not. This should only be called when the feed is visible to
  // the user.
  virtual void RefreshFeedIfNeeded() = 0;
};

#endif  // IOS_CHROME_BROWSER_DISCOVER_FEED_DISCOVER_FEED_REFRESHER_H_

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DISCOVER_FEED_MODEL_DISCOVER_FEED_OBSERVER_H_
#define IOS_CHROME_BROWSER_DISCOVER_FEED_MODEL_DISCOVER_FEED_OBSERVER_H_

#include "base/observer_list.h"

// Observer class for discover feed events.
class DiscoverFeedObserver : public base::CheckedObserver {
 public:
  // Called whenever the FeedProvider Model has changed. At this point all
  // existing Feed ViewControllers are stale and need to be refreshed.
  virtual void OnDiscoverFeedModelRecreated() = 0;
};

#endif  // IOS_CHROME_BROWSER_DISCOVER_FEED_MODEL_DISCOVER_FEED_OBSERVER_H_

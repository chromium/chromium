// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DISCOVER_FEED_MODEL_DISCOVER_FEED_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_DISCOVER_FEED_MODEL_DISCOVER_FEED_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#include "base/scoped_observation.h"
#include "ios/chrome/browser/discover_feed/model/discover_feed_observer.h"
#include "ios/chrome/browser/discover_feed/model/discover_feed_service.h"

// Implement this protocol and pass your implementation into an
// DiscoveFeedObserverBridge object to receive DiscoverFeed observer
// callbacks in Objective-C.
@protocol DiscoverFeedObserverBridgeDelegate <NSObject>

// Called whenever the FeedProvider Model has changed. At this point all
// existing Feed ViewControllers are stale and need to be refreshed.
- (void)discoverFeedModelWasCreated;

@end

// Bridge class that listens for `DiscoverFeedService` notifications and
// passes them to its Objective-C delegate.
class DiscoverFeedObserverBridge : public DiscoverFeedObserver {
 public:
  DiscoverFeedObserverBridge(id<DiscoverFeedObserverBridgeDelegate> observer,
                             DiscoverFeedService* service);
  ~DiscoverFeedObserverBridge() override;

 private:
  // DiscoverFeedObserver implementation:
  void OnDiscoverFeedModelRecreated() override;

  __weak id<DiscoverFeedObserverBridgeDelegate> observer_;
  base::ScopedObservation<DiscoverFeedService, DiscoverFeedObserver>
      scoped_observation_{this};
};

#endif  // IOS_CHROME_BROWSER_DISCOVER_FEED_MODEL_DISCOVER_FEED_OBSERVER_BRIDGE_H_

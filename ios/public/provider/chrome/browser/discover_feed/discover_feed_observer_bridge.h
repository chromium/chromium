// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_DISCOVER_FEED_DISCOVER_FEED_OBSERVER_BRIDGE_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_DISCOVER_FEED_DISCOVER_FEED_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#import "ios/public/provider/chrome/browser/discover_feed/discover_feed_provider.h"

#include "base/scoped_observation.h"

// Implement this protocol and pass your implementation into an
// DiscoveFeedObserverBridge object to receive DiscoverFeed observer
// callbacks in Objective-C.
@protocol DiscoverFeedObserverBridgeDelegate <NSObject>

@optional

// These callbacks follow the semantics of the corresponding
// DiscoverFeedProviderImpl::Observer callbacks. See the comments on
// DiscoverFeedProviderImpl::Observer in discover_feed_provider_impl.h for the
// specification of these semantics.
- (void)onDiscoverFeedModelRecreated;

@end

// Bridge class that listens for |DiscoverFeedProvider| notifications and
// passes them to its Objective-C delegate.
class DiscoverFeedObserverBridge : public DiscoverFeedProvider::Observer {
 public:
  explicit DiscoverFeedObserverBridge(
      id<DiscoverFeedObserverBridgeDelegate> observer);
  ~DiscoverFeedObserverBridge() override;

  DiscoverFeedObserverBridge(const DiscoverFeedObserverBridge&) = delete;
  DiscoverFeedObserverBridge& operator=(const DiscoverFeedObserverBridge&) =
      delete;

 private:
  // DiscoverFeedProvider::Observer implementation.
  void OnDiscoverFeedModelRecreated() override;

  __weak id<DiscoverFeedObserverBridgeDelegate> observer_;
  base::ScopedObservation<DiscoverFeedProvider, DiscoverFeedProvider::Observer>
      scoped_observation_{this};
};

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_DISCOVER_FEED_DISCOVER_FEED_OBSERVER_BRIDGE_H_

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/discover_feed/discover_feed_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

DiscoverFeedObserverBridge::DiscoverFeedObserverBridge(
    id<DiscoverFeedObserverBridgeDelegate> observer,
    DiscoverFeedService* service)
    : observer_(observer) {
  scoped_observation_.Observe(service);
}

DiscoverFeedObserverBridge::~DiscoverFeedObserverBridge() = default;

void DiscoverFeedObserverBridge::OnDiscoverFeedModelRecreated() {
  [observer_ discoverFeedModelWasCreated];
}

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/discover_feed/model/discover_feed_observer_bridge.h"

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

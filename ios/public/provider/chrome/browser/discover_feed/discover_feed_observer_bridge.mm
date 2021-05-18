// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/discover_feed/discover_feed_observer_bridge.h"

#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

DiscoverFeedObserverBridge::DiscoverFeedObserverBridge(
    id<DiscoverFeedObserverBridgeDelegate> observer)
    : observer_(observer) {
  scoped_observation_.Observe(
      ios::GetChromeBrowserProvider()->GetDiscoverFeedProvider());
}

DiscoverFeedObserverBridge::~DiscoverFeedObserverBridge() {}

void DiscoverFeedObserverBridge::OnDiscoverFeedModelRecreated() {
  if ([observer_ respondsToSelector:@selector(onDiscoverFeedModelRecreated)])
    [observer_ onDiscoverFeedModelRecreated];
}

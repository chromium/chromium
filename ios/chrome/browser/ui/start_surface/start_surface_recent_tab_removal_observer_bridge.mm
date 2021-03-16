// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/start_surface/start_surface_recent_tab_removal_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

StartSurfaceRecentTabRemovalObserverBridge::
    StartSurfaceRecentTabRemovalObserverBridge(
        id<StartSurfaceRecentTabRemovalObserving> delegate)
    : delegate_(delegate) {}

StartSurfaceRecentTabRemovalObserverBridge::
    ~StartSurfaceRecentTabRemovalObserverBridge() = default;

void StartSurfaceRecentTabRemovalObserverBridge::MostRecentTabRemoved(
    web::WebState* web_state) {
  const SEL selector = @selector(mostRecentTabWasRemoved:);
  if (![delegate_ respondsToSelector:selector])
    return;

  [delegate_ mostRecentTabWasRemoved:web_state];
}

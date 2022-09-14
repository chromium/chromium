// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/start_surface/start_surface_recent_tab_removal_observer_bridge.h"

#import "base/strings/sys_string_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

StartSurfaceRecentTabObserverBridge::StartSurfaceRecentTabObserverBridge(
    id<StartSurfaceRecentTabObserving> delegate)
    : delegate_(delegate) {}

StartSurfaceRecentTabObserverBridge::~StartSurfaceRecentTabObserverBridge() =
    default;

void StartSurfaceRecentTabObserverBridge::MostRecentTabRemoved(
    web::WebState* web_state) {
  const SEL selector = @selector(mostRecentTabWasRemoved:);
  if (![delegate_ respondsToSelector:selector])
    return;

  [delegate_ mostRecentTabWasRemoved:web_state];
}

void StartSurfaceRecentTabObserverBridge::MostRecentTabFaviconUpdated(
    UIImage* image) {
  const SEL selector = @selector(mostRecentTabFaviconUpdatedWithImage:);
  if (![delegate_ respondsToSelector:selector])
    return;

  [delegate_ mostRecentTabFaviconUpdatedWithImage:image];
}

void StartSurfaceRecentTabObserverBridge::MostRecentTabTitleUpdated(
    const std::u16string& title) {
  const SEL selector = @selector(mostRecentTabTitleWasUpdated:);
  if (![delegate_ respondsToSelector:selector])
    return;

  [delegate_ mostRecentTabTitleWasUpdated:base::SysUTF16ToNSString(title)];
}

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_recent_tab_removal_observer_bridge.h"

#import "base/strings/sys_string_conversions.h"

StartSurfaceRecentTabObserverBridge::StartSurfaceRecentTabObserverBridge(
    id<StartSurfaceRecentTabObserving> delegate)
    : delegate_(delegate) {}

StartSurfaceRecentTabObserverBridge::~StartSurfaceRecentTabObserverBridge() =
    default;

void StartSurfaceRecentTabObserverBridge::MostRecentTabRemoved(
    web::WebState* web_state) {
  [delegate_ mostRecentTabWasRemoved:web_state];
}

void StartSurfaceRecentTabObserverBridge::MostRecentTabFaviconUpdated(
    web::WebState* web_state,
    UIImage* image) {
  [delegate_ mostRecentTab:web_state faviconUpdatedWithImage:image];
}

void StartSurfaceRecentTabObserverBridge::MostRecentTabTitleUpdated(
    web::WebState* web_state,
    const std::u16string& title) {
  [delegate_ mostRecentTab:web_state
           titleWasUpdated:base::SysUTF16ToNSString(title)];
}

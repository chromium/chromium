// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browser_view/model/browser_view_visibility_observer_bridge.h"

#import "base/check.h"

BrowserViewVisibilityObserverBridge::BrowserViewVisibilityObserverBridge(
    id<BrowserViewVisibilityObserving> owner)
    : owner_(owner) {}

BrowserViewVisibilityObserverBridge::~BrowserViewVisibilityObserverBridge() {
  CHECK(!IsInObserverList())
      << "BrowserViewVisibilityObserverBridge needs to be removed from "
         "observer list before their destruction.";
}

void BrowserViewVisibilityObserverBridge::BrowserViewVisibilityStateDidChange(
    BrowserViewVisibilityState current_state,
    BrowserViewVisibilityState previous_state) {
  [owner_ browserViewDidChangeToVisibilityState:current_state
                                      fromState:previous_state];
}

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/ui_bundled/fullscreen/toolbar_ui_observer_bridge.h"

#import <CoreFoundation/CoreFoundation.h>

ToolbarUIObserverBridge::ToolbarUIObserverBridge(
    id<ToolbarUIObserving> observer)
    : observer_(observer) {}

ToolbarUIObserverBridge::~ToolbarUIObserverBridge() {}

void ToolbarUIObserverBridge::OnTopToolbarHeightChanged() {
  [observer_ OnTopToolbarHeightChanged];
}

void ToolbarUIObserverBridge::OnBottomToolbarHeightChanged() {
  [observer_ OnBottomToolbarHeightChanged];
}

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/ui_bundled/fullscreen/toolbars_size_observer_bridge.h"

#import <CoreFoundation/CoreFoundation.h>

ToolbarsSizeObserverBridge::ToolbarsSizeObserverBridge(
    id<ToolbarsSizeObserving> observer)
    : observer_(observer) {}

ToolbarsSizeObserverBridge::~ToolbarsSizeObserverBridge() {}

void ToolbarsSizeObserverBridge::OnTopToolbarHeightChanged() {
  [observer_ OnTopToolbarHeightChanged];
}

void ToolbarsSizeObserverBridge::OnBottomToolbarHeightChanged() {
  [observer_ OnBottomToolbarHeightChanged];
}

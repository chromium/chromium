// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/legacy/ui_bundled/fullscreen/toolbars_size_observer_bridge.h"

#import <CoreFoundation/CoreFoundation.h>

#import "base/check.h"

ToolbarsSizeObserverBridge::ToolbarsSizeObserverBridge(
    id<ToolbarsSizeObserving> observer,
    ToolbarsSize* toolbars_size)
    : observer_(observer), toolbars_size_(toolbars_size) {
  CHECK(observer_);
  CHECK(toolbars_size_);
}

ToolbarsSizeObserverBridge::~ToolbarsSizeObserverBridge() {}

void ToolbarsSizeObserverBridge::OnTopToolbarHeightChanged() {
  if ([observer_ respondsToSelector:@selector
                 (toolbarsSizeDidChangeTopToolbarHeight:)]) {
    // Since `toolbars_size_` is weak, verify it's valid before passing it to
    // the observer.
    CHECK(toolbars_size_);
    [observer_ toolbarsSizeDidChangeTopToolbarHeight:toolbars_size_];
  }
}

void ToolbarsSizeObserverBridge::OnBottomToolbarHeightChanged() {
  if ([observer_ respondsToSelector:@selector
                 (toolbarsSizeDidChangeBottomToolbarHeight:)]) {
    // Since `toolbars_size_` is weak, verify it's valid before passing it to
    // the observer.
    CHECK(toolbars_size_);
    [observer_ toolbarsSizeDidChangeBottomToolbarHeight:toolbars_size_];
  }
}

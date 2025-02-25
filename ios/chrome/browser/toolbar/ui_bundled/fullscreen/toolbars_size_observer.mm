// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/ui_bundled/fullscreen/toolbars_size_observer.h"

#import "base/check.h"

ToolbarsSizeObserver::~ToolbarsSizeObserver() {
  CHECK(!IsInObserverList())
      << "ToolbarsSizeObserver needs to be removed from "
         "ToolbarsSize observer list before their destruction.";
}

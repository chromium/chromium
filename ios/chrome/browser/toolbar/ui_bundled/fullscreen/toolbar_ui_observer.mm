// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/ui_bundled/fullscreen/toolbar_ui_observer.h"

#import "base/check.h"

ToolbarUIObserver::~ToolbarUIObserver() {
  CHECK(!IsInObserverList())
      << "ToolbarUIObserver needs to be removed from "
         "ToolbarUIState observer list before their destruction.";
}

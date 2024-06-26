// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_recent_tab_observer.h"

StartSurfaceRecentTabObserver::~StartSurfaceRecentTabObserver() {
  CHECK(!IsInObserverList())
      << "StartSurfaceRecentTabObserver needs to be removed from "
         "StartSurfaceRecentTab observer list before their destruction.";
}

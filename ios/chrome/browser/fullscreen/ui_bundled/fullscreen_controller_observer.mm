// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_controller_observer.h"

#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_controller.h"

FullscreenControllerObserver::~FullscreenControllerObserver() {
  CHECK(!IsInObserverList())
      << "FullscreenControllerObserver needs to be removed from "
         "FullscreenController observer list before their destruction.";
}

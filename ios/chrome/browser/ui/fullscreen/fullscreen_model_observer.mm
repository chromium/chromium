// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/fullscreen_model_observer.h"

FullscreenModelObserver::~FullscreenModelObserver() {
  CHECK(!IsInObserverList())
      << "FullscreenModelObserver needs to be removed from "
         "FullscreenModel observer list before their destruction.";
}

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/ui/fullscreen_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

FullscreenProvider::FullscreenProvider() = default;

FullscreenProvider::~FullscreenProvider() = default;

void FullscreenProvider::InitializeFullscreen(
    FullscreenController* controller) {}

bool FullscreenProvider::IsInitialized() const {
  return false;
}

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Key used to associate the stored FullscreenControllerImpl.
const char kFullscreenControllerUserDataKey[] = "fullscreen_controller";

}  // namespace

const void* FullscreenController::UserDataKey() {
  return kFullscreenControllerUserDataKey;
}

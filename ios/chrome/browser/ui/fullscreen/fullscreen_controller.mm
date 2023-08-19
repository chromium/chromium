// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"

namespace {

// Key used to associate the stored FullscreenControllerImpl.
const char kFullscreenControllerUserDataKey[] = "fullscreen_controller";

}  // namespace

const void* FullscreenController::UserDataKey() {
  return kFullscreenControllerUserDataKey;
}

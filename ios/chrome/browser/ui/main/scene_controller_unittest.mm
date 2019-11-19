// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main/scene_controller.h"

#import "ios/chrome/browser/ui/main/scene_state.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class SceneControllerTest : public PlatformTest {
 protected:
  SceneControllerTest() {
    scene_state_ = [[SceneState alloc] init];
    scene_controller_ =
        [[SceneController alloc] initWithSceneState:scene_state_];
  }

  SceneController* scene_controller_;
  SceneState* scene_state_;
};

}  // namespace

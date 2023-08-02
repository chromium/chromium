// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/scene/scene_controller.h"

#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "testing/platform_test.h"

namespace {

class SceneControllerTest : public PlatformTest {
 protected:
  SceneControllerTest() {
    scene_state_ = [[SceneState alloc] initWithAppState:nil];
    scene_controller_ =
        [[SceneController alloc] initWithSceneState:scene_state_];
  }

  ~SceneControllerTest() override { [scene_controller_ teardownUI]; }

  SceneController* scene_controller_;
  SceneState* scene_state_;
};

// TODO(crbug.com/1084905): Add a test for keeping validity of detecting a fresh
// open in new window coming from ios dock. 'Dock' is considered the default
// when the new window opening request is external to chrome and unknown.

// Tests that scene controller updates scene state's incognitoContentVisible
// when the relevant application command is called.
TEST_F(SceneControllerTest, UpdatesIncognitoContentVisibility) {
  [scene_controller_ setIncognitoContentVisible:NO];
  EXPECT_FALSE(scene_state_.incognitoContentVisible);
  [scene_controller_ setIncognitoContentVisible:YES];
  EXPECT_TRUE(scene_state_.incognitoContentVisible);
  [scene_controller_ setIncognitoContentVisible:NO];
  EXPECT_FALSE(scene_state_.incognitoContentVisible);
}

}  // namespace

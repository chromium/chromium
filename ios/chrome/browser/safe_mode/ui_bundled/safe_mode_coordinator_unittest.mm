// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_mode/ui_bundled/safe_mode_coordinator.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/test/app/uikit_test_util.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

class SafeModeCoordinatorTest : public PlatformTest {
 public:
  SafeModeCoordinatorTest() {
    scene_state_ = [[SceneState alloc] init];
    scene_state_.window = scoped_key_window_.Get();
  }

 protected:
  // Used to install a ChromeOverlayWindow.
  ScopedKeyWindow scoped_key_window_;
  // The scene state that the agent works with.
  SceneState* scene_state_;
};

TEST_F(SafeModeCoordinatorTest, RootVC) {
  // Expect that starting a safe mode coordinator will populate the root view
  // controller.
  UIViewController* initial_root_view_controller =
      scene_state_.window.rootViewController;
  SafeModeCoordinator* safe_mode_coordinator =
      [[SafeModeCoordinator alloc] initWithSceneState:scene_state_];
  [safe_mode_coordinator start];
  EXPECT_NSNE(scene_state_.window.rootViewController,
              initial_root_view_controller);
}

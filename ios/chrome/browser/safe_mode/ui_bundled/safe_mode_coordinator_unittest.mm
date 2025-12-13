// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_mode/ui_bundled/safe_mode_coordinator.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/ui/chrome_overlay_window/chrome_overlay_window.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

class SafeModeCoordinatorTest : public PlatformTest {
 public:
  SafeModeCoordinatorTest()
      : scene_state_([[SceneState alloc] initWithAppState:nil]) {
    scene_session_mock_ = OCMClassMock([UISceneSession class]);
    OCMStub([scene_session_mock_ persistentIdentifier])
        .andReturn([[NSUUID UUID] UUIDString]);
    scene_mock_ = OCMClassMock([UIWindowScene class]);
    OCMStub([scene_mock_ session]).andReturn(scene_session_mock_);
    scene_state_.scene = scene_mock_;
  }

 protected:
  // The scene state that the agent works with.
  SceneState* scene_state_;
  // Mock for scene_state_'s underlying UIWindowScene.
  id scene_mock_;
  // Mock for scene_mock_'s underlying UISceneSession.
  id scene_session_mock_;
};

TEST_F(SafeModeCoordinatorTest, RootVC) {
  // Expect that starting a safe mode coordinator will populate the root view
  // controller.
  UIWindow* window = [[ChromeOverlayWindow alloc] init];
  ;

  id applicationWindowMock = nil;
  OCMStub([scene_mock_ windows]).andReturn(@[ window ]);

  UIViewController* initial_root_view_controller =
      scene_state_.window.rootViewController;
  SafeModeCoordinator* safe_mode_coordinator =
      [[SafeModeCoordinator alloc] initWithSceneState:scene_state_];
  [safe_mode_coordinator start];
  EXPECT_NE(scene_state_.window.rootViewController,
            initial_root_view_controller);

  [applicationWindowMock stopMocking];
}

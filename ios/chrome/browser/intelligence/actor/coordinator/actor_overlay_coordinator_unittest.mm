// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/coordinator/actor_overlay_coordinator.h"

#import <UIKit/UIKit.h>

#import <memory>

#import "ios/chrome/browser/intelligence/actor/ui/actor_overlay_view_controller.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_scene_agent.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

class ActorOverlayCoordinatorTest : public PlatformTest {
 public:
  ActorOverlayCoordinatorTest(const ActorOverlayCoordinatorTest&) = delete;
  ActorOverlayCoordinatorTest& operator=(const ActorOverlayCoordinatorTest&) =
      delete;

 protected:
  ActorOverlayCoordinatorTest() {
    profile_ = TestProfileIOS::Builder().Build();
    scene_state_ = [[SceneState alloc] init];
    LayoutGuideSceneAgent* layout_guide_scene_agent =
        [[LayoutGuideSceneAgent alloc] init];
    [scene_state_ addAgent:layout_guide_scene_agent];
    browser_ = std::make_unique<TestBrowser>(profile_.get(), scene_state_);
    base_view_controller_ = [[UIViewController alloc] init];
  }

  // Task environment that manages the message loops and threads for testing.
  web::WebTaskEnvironment task_environment_;
  // Profile instance used to initialize the browser.
  std::unique_ptr<TestProfileIOS> profile_;
  // `SceneState` used to attach `LayoutGuideSceneAgent`.
  SceneState* scene_state_ = nil;
  // Browser instance containing the active tab undergoing test.
  std::unique_ptr<TestBrowser> browser_;
  // Base view controller used to present the coordinator's UI.
  UIViewController* base_view_controller_ = nil;
};

// Test that `start()` and `stop()` correctly add/remove the child view
// controller and its view.
TEST_F(ActorOverlayCoordinatorTest, StartAndStopLifecycle) {
  web::FakeWebState fake_web_state;
  ActorOverlayCoordinator* coordinator = [[ActorOverlayCoordinator alloc]
      initWithBaseViewController:base_view_controller_
                         browser:browser_.get()
                        webState:&fake_web_state];

  EXPECT_NE(coordinator, nil);

  // Before starting, no child view controllers.
  EXPECT_EQ(base_view_controller_.childViewControllers.count, 0u);

  // Start the coordinator.
  [coordinator start];

  // Verify that an `ActorOverlayViewController` was added as a child.
  EXPECT_EQ(base_view_controller_.childViewControllers.count, 1u);
  UIViewController* child =
      base_view_controller_.childViewControllers.firstObject;
  EXPECT_TRUE([child isKindOfClass:[ActorOverlayViewController class]]);

  // Verify that the child's view was added as a subview.
  EXPECT_EQ(child.view.superview, base_view_controller_.view);
  EXPECT_TRUE([base_view_controller_.view.subviews containsObject:child.view]);

  // Stop the coordinator.
  [coordinator stop];

  // Verify everything was cleaned up.
  EXPECT_EQ(base_view_controller_.childViewControllers.count, 0u);
  EXPECT_EQ(child.view.superview, nil);
  EXPECT_FALSE([base_view_controller_.view.subviews containsObject:child.view]);
}

// Test that calling `start()` multiple times consecutively does not result in
// duplicate child view controllers or duplicate subviews.
TEST_F(ActorOverlayCoordinatorTest, StartReentrance) {
  web::FakeWebState fake_web_state;
  ActorOverlayCoordinator* coordinator = [[ActorOverlayCoordinator alloc]
      initWithBaseViewController:base_view_controller_
                         browser:browser_.get()
                        webState:&fake_web_state];

  EXPECT_NE(coordinator, nil);

  // Call `start()` twice.
  [coordinator start];
  [coordinator start];

  // Verify that only a single `ActorOverlayViewController` child and view were
  // added.
  EXPECT_EQ(base_view_controller_.childViewControllers.count, 1u);
  UIViewController* child =
      base_view_controller_.childViewControllers.firstObject;
  EXPECT_TRUE([child isKindOfClass:[ActorOverlayViewController class]]);

  EXPECT_EQ(child.view.superview, base_view_controller_.view);
  EXPECT_TRUE([base_view_controller_.view.subviews containsObject:child.view]);

  // Stop the coordinator.
  [coordinator stop];
  EXPECT_EQ(base_view_controller_.childViewControllers.count, 0u);
  EXPECT_EQ(child.view.superview, nil);
  EXPECT_FALSE([base_view_controller_.view.subviews containsObject:child.view]);
}

}  // namespace

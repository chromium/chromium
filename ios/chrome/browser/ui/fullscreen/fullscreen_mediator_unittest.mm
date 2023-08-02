// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/fullscreen_mediator.h"

#import "ios/chrome/browser/ui/fullscreen/fullscreen_model.h"
#import "ios/chrome/browser/ui/fullscreen/test/fullscreen_model_test_util.h"
#import "ios/chrome/browser/ui/fullscreen/test/test_fullscreen_controller.h"
#import "ios/chrome/browser/ui/fullscreen/test/test_fullscreen_controller_observer.h"
#import "ios/chrome/browser/ui/fullscreen/test/test_fullscreen_mediator.h"
#import "testing/platform_test.h"

// Test fixture for FullscreenMediator.
class FullscreenMediatorTest : public PlatformTest {
 public:
  FullscreenMediatorTest()
      : PlatformTest(), controller_(&model_), mediator_(&controller_, &model_) {
    SetUpFullscreenModelForTesting(&model_, 100);
    mediator_.AddObserver(&observer_);
  }
  ~FullscreenMediatorTest() override {
    mediator_.Disconnect();
    mediator_.RemoveObserver(&observer_);
    EXPECT_TRUE(observer_.is_shut_down());
  }

  FullscreenController* controller() {
    // TestFullscreenControllerObserver doesn't use the FullscreenController
    // passed to its observer methods, so use a dummy pointer.
    static void* kFullscreenController = &kFullscreenController;
    return reinterpret_cast<FullscreenController*>(kFullscreenController);
  }
  FullscreenModel& model() { return model_; }
  TestFullscreenControllerObserver& observer() { return observer_; }

 private:
  FullscreenModel model_;
  TestFullscreenController controller_;
  TestFullscreenMediator mediator_;
  TestFullscreenControllerObserver observer_;
};

// Tests that progress and scroll end animator are correctly forwarded to the
// observer.
TEST_F(FullscreenMediatorTest, ObserveProgressAndScrollEnd) {
  SimulateFullscreenUserScrollForProgress(&model(), 0.5);
  EXPECT_EQ(observer().progress(), 0.5);
  FullscreenAnimator* animator = observer().animator();
  EXPECT_TRUE(animator);
  EXPECT_EQ(animator.startProgress, 0.5);
  EXPECT_EQ(animator.finalProgress, 1.0);
}

// Tests that the enabled state is correctly forwarded to the observer.
TEST_F(FullscreenMediatorTest, ObserveEnabledState) {
  EXPECT_TRUE(observer().enabled());
  model().IncrementDisabledCounter();
  EXPECT_FALSE(observer().enabled());
  model().DecrementDisabledCounter();
  EXPECT_TRUE(observer().enabled());
}

// Tests that changes to the model's toolbar heights are forwarded to observers.
TEST_F(FullscreenMediatorTest, ObserveViewportInsets) {
  const CGFloat kExpandedTopToolbarHeight = 100.0;
  const CGFloat kCollapsedTopToolbarHeight = 50.0;
  const CGFloat kExpandedBottomToolbarHeight = 60.0;
  const CGFloat kCollapsedBottomToolbarHeight = 1.0;
  model().SetExpandedTopToolbarHeight(kExpandedTopToolbarHeight);
  model().SetCollapsedTopToolbarHeight(kCollapsedTopToolbarHeight);
  model().SetExpandedBottomToolbarHeight(kExpandedBottomToolbarHeight);
  model().SetCollapsedBottomToolbarHeight(kCollapsedBottomToolbarHeight);
  EXPECT_TRUE(UIEdgeInsetsEqualToEdgeInsets(
      observer().min_viewport_insets(),
      UIEdgeInsetsMake(kCollapsedTopToolbarHeight, 0,
                       kCollapsedBottomToolbarHeight, 0)));
  EXPECT_TRUE(UIEdgeInsetsEqualToEdgeInsets(
      observer().max_viewport_insets(),
      UIEdgeInsetsMake(kExpandedTopToolbarHeight, 0,
                       kExpandedBottomToolbarHeight, 0)));
  EXPECT_TRUE(UIEdgeInsetsEqualToEdgeInsets(
      observer().current_viewport_insets(),
      UIEdgeInsetsMake(kExpandedTopToolbarHeight, 0,
                       kExpandedBottomToolbarHeight, 0)));
}

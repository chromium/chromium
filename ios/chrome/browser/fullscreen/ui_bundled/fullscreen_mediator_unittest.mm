// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_mediator.h"

#import "base/test/task_environment.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_model.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/test/fullscreen_model_test_util.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/test/test_fullscreen_controller.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/test/test_fullscreen_controller_observer.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/test/test_fullscreen_mediator.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/toolbar/ui_bundled/fullscreen/toolbars_size.h"
#import "ios/chrome/browser/toolbar/ui_bundled/fullscreen/toolbars_size_browser_agent.h"
#import "testing/platform_test.h"

// Test fixture for FullscreenMediator.
class FullscreenMediatorTest : public PlatformTest {
 public:
  FullscreenMediatorTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    ToolbarsSizeBrowserAgent::CreateForBrowser(browser_.get());
    TestFullscreenController::CreateForBrowser(browser_.get());
    mediator_ = std::make_unique<TestFullscreenMediator>(controller(), model());
    observer_ = std::make_unique<TestFullscreenControllerObserver>();
    SetUpFullscreenModelForTesting(model(), 100);
    mediator_->AddObserver(observer_.get());
  }
  ~FullscreenMediatorTest() override {
    mediator_->Disconnect();
    mediator_->RemoveObserver(observer_.get());
    EXPECT_TRUE(observer_->is_shut_down());
  }

  TestFullscreenController* controller() {
    return TestFullscreenController::FromBrowser(browser_.get());
  }

  FullscreenModel* model() { return controller()->getModel(); }
  TestFullscreenControllerObserver& observer() { return *observer_; }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  std::unique_ptr<TestFullscreenMediator> mediator_;
  std::unique_ptr<TestFullscreenControllerObserver> observer_;
};

// Tests that the browser remains in fullscreen after the first scroll to
// bottom.
TEST_F(FullscreenMediatorTest, StaysFullscreenOnFirstScrollToBottom) {
  SimulateFullscreenUserScrollForProgress(model(), 1.0);
  EXPECT_EQ(model()->progress(), 1.0);

  SimulateScrollToBottom(model());

  EXPECT_EQ(model()->progress(), 0.0);
  EXPECT_FALSE(observer().animator());
}

// Tests that the browser exits fullscreen on the second scroll to the bottom.
TEST_F(FullscreenMediatorTest, ExitsFullscreenOnSecondScrollToBottom) {
  SimulateFullscreenUserScrollForProgress(model(), 1.0);
  SimulateScrollToBottom(model());

  SimulateFullscreenUserScrollForProgress(model(), 0.9);

  SimulateScrollToBottom(model());

  FullscreenAnimator* animator = observer().animator();
  EXPECT_TRUE(animator);
  EXPECT_EQ(animator.finalProgress, 1.0);
}

// Tests that the enabled state is correctly forwarded to the observer.
TEST_F(FullscreenMediatorTest, ObserveEnabledState) {
  EXPECT_TRUE(observer().enabled());
  model()->IncrementDisabledCounter();
  EXPECT_FALSE(observer().enabled());
  model()->DecrementDisabledCounter();
  EXPECT_TRUE(observer().enabled());
}

// Tests that changes to the model's toolbar heights are forwarded to observers.
TEST_F(FullscreenMediatorTest, ObserveViewportInsets) {
  const CGFloat kExpandedTopToolbarHeight = 100.0;
  const CGFloat kCollapsedTopToolbarHeight = 50.0;
  const CGFloat kExpandedBottomToolbarHeight = 60.0;
  const CGFloat kCollapsedBottomToolbarHeight = 1.0;

  ToolbarsSize* toolbarsSize = [[ToolbarsSize alloc]
      initWithCollapsedTopToolbarHeight:kCollapsedTopToolbarHeight
               expandedTopToolbarHeight:kExpandedTopToolbarHeight
            expandedBottomToolbarHeight:kExpandedBottomToolbarHeight
           collapsedBottomToolbarHeight:kCollapsedBottomToolbarHeight];
  model()->SetToolbarsSize(toolbarsSize);
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

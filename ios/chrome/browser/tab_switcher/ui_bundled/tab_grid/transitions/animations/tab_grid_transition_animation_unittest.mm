// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/transitions/animations/tab_grid_transition_animation.h"

#import <UIKit/UIKit.h>

#import <memory>

#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "ios/chrome/browser/shared/public/commands/tab_grid_commands.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/transitions/animations/grid_to_tab_animation.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/transitions/animations/tab_grid_animation_parameters.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/transitions/animations/tab_grid_reduced_animation.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/transitions/animations/tab_to_grid_animation.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/test/ios/ui_image_test_utils.h"

namespace {

// Timeout for waiting for transition animations to complete.
constexpr base::TimeDelta kAnimationTimeout = base::Seconds(4);

// Geometry constants for test views and frames.
constexpr CGFloat kGridCellX = 20.0;
constexpr CGFloat kGridCellY = 40.0;
constexpr CGFloat kGridCellWidth = 100.0;
constexpr CGFloat kGridCellHeight = 150.0;
constexpr CGFloat kToolbarHeight = 50.0;
constexpr CGFloat kSnapshotWidth = 300.0;
constexpr CGFloat kSnapshotHeight = 500.0;
constexpr CGFloat kInitialScale = 0.5;

}  // namespace

// Test fixture for tab grid transition animations.
class TabGridTransitionAnimationTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    active_grid_view_controller_ = [[UIViewController alloc] init];
    pinned_tabs_view_controller_ = [[UIViewController alloc] init];
    tab_grid_handler_ = OCMProtocolMock(@protocol(TabGridCommands));
  }

  void TearDown() override {
    tab_grid_handler_ = nil;
    pinned_tabs_view_controller_ = nil;
    active_grid_view_controller_ = nil;
    PlatformTest::TearDown();
  }

  // Helper to create a standardized animated view matching window bounds.
  UIView* CreateAnimatedView() {
    UIView* animated_view =
        [[UIView alloc] initWithFrame:scoped_key_window_.Get().bounds];
    animated_view.transform =
        CGAffineTransformMakeScale(kInitialScale, kInitialScale);
    [scoped_key_window_.Get() addSubview:animated_view];
    return animated_view;
  }

  // Creates and returns standardized animation parameters for testing.
  TabGridAnimationParameters* CreateAnimationParameters(
      UIView* animated_view,
      bool incognito = false,
      bool active_cell_pinned = false,
      bool top_toolbar_hidden = false,
      bool should_scale_top_toolbar = true) {
    CGRect destination_frame = animated_view.bounds;
    CGFloat window_width = destination_frame.size.width;
    CGFloat window_height = destination_frame.size.height;
    CGRect origin_frame =
        CGRectMake(kGridCellX, kGridCellY, kGridCellWidth, kGridCellHeight);

    UIView* top_toolbar = [[UIView alloc]
        initWithFrame:CGRectMake(0, 0, window_width, kToolbarHeight)];
    UIView* bottom_toolbar = [[UIView alloc]
        initWithFrame:CGRectMake(0, window_height - kToolbarHeight,
                                 window_width, kToolbarHeight)];

    UIImage* content_snapshot =
        ui::test::uiimage_utils::UIImageWithSizeAndSolidColor(
            CGSizeMake(kSnapshotWidth, kSnapshotHeight),
            [UIColor colorNamed:kBlueColor]);

    return [[TabGridAnimationParameters alloc]
         initWithDestinationFrame:destination_frame
                      originFrame:origin_frame
                       activeGrid:active_grid_view_controller_
                       pinnedTabs:pinned_tabs_view_controller_
                 activeCellPinned:active_cell_pinned
                     animatedView:animated_view
                  contentSnapshot:content_snapshot
                 topToolbarHeight:kToolbarHeight
              bottomToolbarHeight:kToolbarHeight
           topToolbarSnapshotView:top_toolbar
        bottomToolbarSnapshotView:bottom_toolbar
            shouldScaleTopToolbar:should_scale_top_toolbar
                        incognito:incognito
                 topToolbarHidden:top_toolbar_hidden
                   commandHandler:tab_grid_handler_];
  }

  // Helper to run an animation to completion and verify standard cleanup
  // invariants.
  void RunAnimationAndVerifyCleanup(id<TabGridTransitionAnimation> animation,
                                    UIView* animated_view) {
    __block bool completion_called = false;
    [animation animateWithCompletion:^{
      completion_called = true;
    }];

    EXPECT_TRUE(
        base::test::ios::WaitUntilConditionOrTimeout(kAnimationTimeout, ^bool {
          return completion_called;
        }));

    EXPECT_TRUE(CGAffineTransformEqualToTransform(animated_view.transform,
                                                  CGAffineTransformIdentity));
    EXPECT_EQ(animated_view.layer.mask, nil);
    EXPECT_EQ(animated_view.subviews.count, 0u);
    EXPECT_EQ(animated_view.alpha, 1.0);
  }

  // Window used to host test views in a valid hierarchy.
  ScopedKeyWindow scoped_key_window_;

  // View controllers used as grid targets in transition parameters.
  UIViewController* active_grid_view_controller_;
  UIViewController* pinned_tabs_view_controller_;

  // Mock handler for tab grid commands.
  id<TabGridCommands> tab_grid_handler_;
};

// Tests that GridToTabAnimation resets the animated view's transform to
// identity and clears its mask upon completion in regular mode.
TEST_F(TabGridTransitionAnimationTest,
       GridToTabAnimationResetsTransformOnCompletion) {
  UIView* animated_view = CreateAnimatedView();
  TabGridAnimationParameters* params = CreateAnimationParameters(animated_view);
  GridToTabAnimation* animation =
      [[GridToTabAnimation alloc] initWithAnimationParameters:params];

  RunAnimationAndVerifyCleanup(animation, animated_view);
}

// Tests that TabToGridAnimation resets the animated view's transform to
// identity and clears its mask upon completion in regular mode.
TEST_F(TabGridTransitionAnimationTest,
       TabToGridAnimationResetsTransformOnCompletion) {
  UIView* animated_view = CreateAnimatedView();
  TabGridAnimationParameters* params = CreateAnimationParameters(animated_view);
  TabToGridAnimation* animation =
      [[TabToGridAnimation alloc] initWithAnimationParameters:params];

  RunAnimationAndVerifyCleanup(animation, animated_view);
}

// Tests that GridToTabAnimation resets transform and cleans up subviews in
// Incognito mode.
TEST_F(TabGridTransitionAnimationTest,
       GridToTabAnimationIncognitoResetsTransformOnCompletion) {
  UIView* animated_view = CreateAnimatedView();
  TabGridAnimationParameters* params =
      CreateAnimationParameters(animated_view, /*incognito=*/true);
  GridToTabAnimation* animation =
      [[GridToTabAnimation alloc] initWithAnimationParameters:params];

  RunAnimationAndVerifyCleanup(animation, animated_view);
}

// Tests that TabToGridAnimation resets transform and cleans up subviews in
// Incognito mode.
TEST_F(TabGridTransitionAnimationTest,
       TabToGridAnimationIncognitoResetsTransformOnCompletion) {
  UIView* animated_view = CreateAnimatedView();
  TabGridAnimationParameters* params =
      CreateAnimationParameters(animated_view, /*incognito=*/true);
  TabToGridAnimation* animation =
      [[TabToGridAnimation alloc] initWithAnimationParameters:params];

  RunAnimationAndVerifyCleanup(animation, animated_view);
}

// Tests that GridToTabAnimation resets transform when transitioning from a
// pinned tab cell.
TEST_F(TabGridTransitionAnimationTest,
       GridToTabAnimationPinnedCellResetsTransformOnCompletion) {
  UIView* animated_view = CreateAnimatedView();
  TabGridAnimationParameters* params = CreateAnimationParameters(
      animated_view, /*incognito=*/false, /*active_cell_pinned=*/true);
  GridToTabAnimation* animation =
      [[GridToTabAnimation alloc] initWithAnimationParameters:params];

  RunAnimationAndVerifyCleanup(animation, animated_view);
}

// Tests that TabToGridAnimation resets transform when transitioning to a
// pinned tab cell.
TEST_F(TabGridTransitionAnimationTest,
       TabToGridAnimationPinnedCellResetsTransformOnCompletion) {
  UIView* animated_view = CreateAnimatedView();
  TabGridAnimationParameters* params = CreateAnimationParameters(
      animated_view, /*incognito=*/false, /*active_cell_pinned=*/true);
  TabToGridAnimation* animation =
      [[TabToGridAnimation alloc] initWithAnimationParameters:params];

  RunAnimationAndVerifyCleanup(animation, animated_view);
}

// Tests that GridToTabAnimation resets transform when top toolbar is hidden and
// unscaled.
TEST_F(TabGridTransitionAnimationTest,
       GridToTabAnimationHiddenToolbarResetsTransformOnCompletion) {
  UIView* animated_view = CreateAnimatedView();
  TabGridAnimationParameters* params = CreateAnimationParameters(
      animated_view, /*incognito=*/false, /*active_cell_pinned=*/false,
      /*top_toolbar_hidden=*/true, /*should_scale_top_toolbar=*/false);
  GridToTabAnimation* animation =
      [[GridToTabAnimation alloc] initWithAnimationParameters:params];

  RunAnimationAndVerifyCleanup(animation, animated_view);
}

// Tests that TabGridReducedAnimation resets transform, alpha, and clipsToBounds
// upon completion during presentation.
TEST_F(TabGridTransitionAnimationTest,
       TabGridReducedAnimationPresentationResetsTransform) {
  UIView* animated_view = CreateAnimatedView();
  animated_view.alpha = 0.5;
  TabGridReducedAnimation* animation =
      [[TabGridReducedAnimation alloc] initWithAnimatedView:animated_view
                                             beingPresented:YES];

  RunAnimationAndVerifyCleanup(animation, animated_view);
}

// Tests that TabGridReducedAnimation resets transform, alpha, and clipsToBounds
// upon completion during dismissal.
TEST_F(TabGridTransitionAnimationTest,
       TabGridReducedAnimationDismissalResetsTransform) {
  UIView* animated_view = CreateAnimatedView();
  animated_view.alpha = 0.5;
  TabGridReducedAnimation* animation =
      [[TabGridReducedAnimation alloc] initWithAnimatedView:animated_view
                                             beingPresented:NO];

  RunAnimationAndVerifyCleanup(animation, animated_view);
}

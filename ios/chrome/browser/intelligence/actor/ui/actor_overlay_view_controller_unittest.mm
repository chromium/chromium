// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/ui/actor_overlay_view_controller.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/app_bar/ui/app_bar_constants.h"
#import "ios/chrome/browser/intelligence/actor/ui/actor_overlay_glow_view.h"
#import "ios/chrome/browser/intelligence/actor/ui/actor_overlay_glow_view_data.h"
#import "ios/chrome/browser/intelligence/actor/ui/actor_overlay_scrim_view.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/layout_state_passkey.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/layout_state_test_passkey_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/scene_layout_state.h"
#import "ios/chrome/browser/shared/ui/util/layout_constants.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "ui/base/device_form_factor.h"

namespace {

using layout_state::LayoutStateTestPassKeyFactory;

LayoutStateScenePassKey ScenePassKey() {
  return LayoutStateTestPassKeyFactory::CreateSceneKey();
}

class ActorOverlayViewControllerTest : public PlatformTest {
 public:
  ActorOverlayViewControllerTest() {
    layout_guide_center_ = [[LayoutGuideCenter alloc] init];
    layout_state_ = [[SceneLayoutState alloc] init];
  }

  ActorOverlayViewControllerTest(const ActorOverlayViewControllerTest&) =
      delete;
  ActorOverlayViewControllerTest& operator=(
      const ActorOverlayViewControllerTest&) = delete;

 protected:
  // PlatformTest:
  void TearDown() override {
    scoped_key_window_.Get().rootViewController = nil;
    view_controller_.layoutState = nil;
    view_controller_ = nil;
    PlatformTest::TearDown();
  }

  void ExpectCornerRadii(UIView* glow_view,
                         CGFloat top_left,
                         CGFloat top_right,
                         CGFloat bottom_left,
                         CGFloat bottom_right) const {
    ActorOverlayGlowView* actor_glow_view =
        static_cast<ActorOverlayGlowView*>(glow_view);
    CornerRadii expected_radii(top_left, top_right, bottom_left, bottom_right);
    EXPECT_EQ(actor_glow_view.cornerRadii, expected_radii);
  }

  // Key window helper.
  ScopedKeyWindow scoped_key_window_;
  // Center for browser layout guides.
  LayoutGuideCenter* layout_guide_center_ = nil;
  // Layout state used to observe app bar position changes.
  SceneLayoutState* layout_state_ = nil;
  // View controller under test.
  ActorOverlayViewController* view_controller_ = nil;
  // Task environment for mocking time and managing the run loop.
  web::WebTaskEnvironment task_environment_;
};

// Test that `ActorOverlayViewController` sets up the subviews correctly.
TEST_F(ActorOverlayViewControllerTest, SetupViewAndSubviews) {
  view_controller_ = [[ActorOverlayViewController alloc]
      initWithBrowserLayoutGuideCenter:layout_guide_center_
                            scrimColor:[UIColor blackColor]
                             glowColor:[UIColor whiteColor]];
  view_controller_.layoutState = layout_state_;

  // Force view load.
  EXPECT_NE(view_controller_.view, nil);

  // Use KVC to retrieve private subviews.
  UIView* scrim_view = [view_controller_ valueForKey:@"_scrimView"];
  UIView* glow_view = [view_controller_ valueForKey:@"_glowView"];

  ASSERT_NE(scrim_view, nil);
  ASSERT_NE(glow_view, nil);

  EXPECT_TRUE([scrim_view isKindOfClass:[ActorOverlayScrimView class]]);
  EXPECT_TRUE([glow_view isKindOfClass:[ActorOverlayGlowView class]]);

  // Glow view should be visible.
  EXPECT_FALSE(glow_view.hidden);
}

// Test that `LayoutState` `appBarPosition` default and bottom updates change
// constraints and corner radii accordingly.
TEST_F(ActorOverlayViewControllerTest,
       LayoutStateChangesConstraintsAndRadiiDefaultBottom) {
  view_controller_ = [[ActorOverlayViewController alloc]
      initWithBrowserLayoutGuideCenter:layout_guide_center_
                            scrimColor:[UIColor blackColor]
                             glowColor:[UIColor whiteColor]];
  view_controller_.layoutState = layout_state_;

  // Load view.
  UIView* view = view_controller_.view;
  ASSERT_NE(view, nil);

  NSLayoutConstraint* bottom_constraint =
      [view_controller_ valueForKey:@"_glowBottomConstraint"];
  NSLayoutConstraint* leading_constraint =
      [view_controller_ valueForKey:@"_glowLeadingConstraint"];
  NSLayoutConstraint* trailing_constraint =
      [view_controller_ valueForKey:@"_glowTrailingConstraint"];

  ASSERT_NE(bottom_constraint, nil);
  ASSERT_NE(leading_constraint, nil);
  ASSERT_NE(trailing_constraint, nil);

  UIView* glow_view = [view_controller_ valueForKey:@"_glowView"];
  ASSERT_NE(glow_view, nil);

  const CGFloat device_radius = DeviceCornerRadius();
  const BOOL is_tablet =
      ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET;
  const CGFloat expected_top_left = is_tablet ? 0.0 : device_radius;
  const CGFloat expected_top_right = is_tablet ? 0.0 : device_radius;

  // Default layout state is `kNone`.
  EXPECT_EQ(bottom_constraint.constant, 0.0);
  EXPECT_EQ(leading_constraint.constant, 0.0);
  EXPECT_EQ(trailing_constraint.constant, 0.0);
  ExpectCornerRadii(glow_view, expected_top_left, expected_top_right,
                    device_radius, device_radius);

  // Change to Bottom position.
  [layout_state_ setAppBarPosition:AppBarPosition::kBottom
                           passKey:ScenePassKey()];

  EXPECT_EQ(bottom_constraint.constant, -AppBarHeightPortrait());
  EXPECT_EQ(leading_constraint.constant, 0.0);
  EXPECT_EQ(trailing_constraint.constant, 0.0);
  ExpectCornerRadii(glow_view, expected_top_left, expected_top_right,
                    kAppBarCornerRadius, kAppBarCornerRadius);
}

// Test that `LayoutState` `appBarPosition` left and right updates change
// constraints and corner radii accordingly.
TEST_F(ActorOverlayViewControllerTest,
       LayoutStateChangesConstraintsAndRadiiLeftRight) {
  view_controller_ = [[ActorOverlayViewController alloc]
      initWithBrowserLayoutGuideCenter:layout_guide_center_
                            scrimColor:[UIColor blackColor]
                             glowColor:[UIColor whiteColor]];
  view_controller_.layoutState = layout_state_;

  // Load view.
  UIView* view = view_controller_.view;
  ASSERT_NE(view, nil);

  NSLayoutConstraint* bottom_constraint =
      [view_controller_ valueForKey:@"_glowBottomConstraint"];
  NSLayoutConstraint* leading_constraint =
      [view_controller_ valueForKey:@"_glowLeadingConstraint"];
  NSLayoutConstraint* trailing_constraint =
      [view_controller_ valueForKey:@"_glowTrailingConstraint"];

  ASSERT_NE(bottom_constraint, nil);
  ASSERT_NE(leading_constraint, nil);
  ASSERT_NE(trailing_constraint, nil);

  UIView* glow_view = [view_controller_ valueForKey:@"_glowView"];
  ASSERT_NE(glow_view, nil);

  const CGFloat device_radius = DeviceCornerRadius();
  const BOOL is_tablet =
      ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET;
  const CGFloat expected_top_left = is_tablet ? 0.0 : device_radius;
  const CGFloat expected_top_right = is_tablet ? 0.0 : device_radius;

  // Change to Left position.
  [layout_state_ setAppBarPosition:AppBarPosition::kLeft
                           passKey:ScenePassKey()];

  EXPECT_EQ(bottom_constraint.constant, 0.0);
  EXPECT_EQ(leading_constraint.constant, AppBarHeightLandscape());
  EXPECT_EQ(trailing_constraint.constant, 0.0);
  ExpectCornerRadii(glow_view, kAppBarCornerRadius, expected_top_right,
                    kAppBarCornerRadius, device_radius);

  // Change to Right position.
  [layout_state_ setAppBarPosition:AppBarPosition::kRight
                           passKey:ScenePassKey()];

  EXPECT_EQ(bottom_constraint.constant, 0.0);
  EXPECT_EQ(leading_constraint.constant, 0.0);
  EXPECT_EQ(trailing_constraint.constant, -AppBarHeightLandscape());
  ExpectCornerRadii(glow_view, expected_top_left, kAppBarCornerRadius,
                    device_radius, kAppBarCornerRadius);
}

}  // namespace

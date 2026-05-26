// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/legacy/ui_bundled/secondary_toolbar_view_controller.h"

#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/toolbar/legacy/ui_bundled/buttons/legacy_toolbar_button_factory.h"
#import "ios/chrome/browser/toolbar/legacy/ui_bundled/buttons/toolbar_style.h"
#import "testing/platform_test.h"

@interface SecondaryToolbarViewController (Testing)
@property(nonatomic) BOOL locationIndicatorActive;
- (void)collapsedToolbarButtonTapped;
@end

class SecondaryToolbarViewControllerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    button_factory_ = [[LegacyToolbarButtonFactory alloc]
        initWithStyle:ToolbarStyle::kNormal];
    layout_guide_center_ = [[LayoutGuideCenter alloc] init];

    view_controller_ = [[SecondaryToolbarViewController alloc] init];
    view_controller_.buttonFactory = button_factory_;
    view_controller_.layoutGuideCenter = layout_guide_center_;

    // Force view loading to initialize subviews and constraints.
    (void)view_controller_.view;
  }

  void TearDown() override {
    view_controller_ = nil;
    button_factory_ = nil;
    layout_guide_center_ = nil;
    PlatformTest::TearDown();
  }

  SecondaryToolbarViewController* view_controller_;
  LegacyToolbarButtonFactory* button_factory_;
  LayoutGuideCenter* layout_guide_center_;
};

// Tests that the controller initializes correctly.
TEST_F(SecondaryToolbarViewControllerTest, Initialization) {
  EXPECT_NE(view_controller_, nil);
  EXPECT_FALSE(view_controller_.hasOmnibox);
  EXPECT_FALSE(view_controller_.locationIndicatorActive);
}

// Tests that transitioning locationBarViewController to nil when
// locationIndicatorActive is YES correctly resets locationIndicatorActive to
// NO.
TEST_F(SecondaryToolbarViewControllerTest,
       ResetsLocationIndicatorActiveOnLocationBarRemoval) {
  UIViewController* location_bar = [[UIViewController alloc] init];
  view_controller_.locationBarViewController = location_bar;
  EXPECT_TRUE(view_controller_.hasOmnibox);

  view_controller_.locationIndicatorActive = YES;
  EXPECT_TRUE(view_controller_.locationIndicatorActive);

  // Transition locationBarViewController to nil.
  view_controller_.locationBarViewController = nil;

  // hasOmnibox should be NO, and locationIndicatorActive should have been reset
  // to NO.
  EXPECT_FALSE(view_controller_.hasOmnibox);
  EXPECT_FALSE(view_controller_.locationIndicatorActive);

  // Tapping the collapsed toolbar button should not crash now because
  // locationIndicatorActive is NO.
  [view_controller_ collapsedToolbarButtonTapped];
}

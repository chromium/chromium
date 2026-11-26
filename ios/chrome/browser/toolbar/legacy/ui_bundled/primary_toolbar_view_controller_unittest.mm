// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/legacy/ui_bundled/primary_toolbar_view_controller.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/toolbar/legacy/ui_bundled/buttons/legacy_toolbar_button.h"
#import "ios/chrome/browser/toolbar/legacy/ui_bundled/buttons/legacy_toolbar_button_factory.h"
#import "ios/chrome/browser/toolbar/legacy/ui_bundled/buttons/toolbar_button_visibility_configuration.h"
#import "ios/chrome/browser/toolbar/legacy/ui_bundled/buttons/toolbar_style.h"
#import "ios/chrome/browser/toolbar/legacy/ui_bundled/primary_toolbar_view.h"
#import "ios/chrome/browser/toolbar/legacy/ui_bundled/public/toolbar_constants.h"
#import "ios/chrome/browser/toolbar/legacy/ui_bundled/public/toolbar_type.h"
#import "ios/chrome/test/app/uikit_test_util.h"
#import "testing/platform_test.h"

namespace {

// Returns a button factory for a toolbar of `type`.
LegacyToolbarButtonFactory* ButtonFactory(ToolbarType type) {
  LegacyToolbarButtonFactory* factory =
      [[LegacyToolbarButtonFactory alloc] initWithStyle:ToolbarStyle::kNormal];
  factory.visibilityConfiguration =
      [[ToolbarButtonVisibilityConfiguration alloc] initWithType:type];
  return factory;
}

// Overrides `trait_overrides`' size classes to compact width and height,
// where the toolbar isn't split (e.g. iPhone Duo folded in landscape).
void OverrideToCompactHeight(id<UITraitOverrides> trait_overrides) {
  trait_overrides.horizontalSizeClass = UIUserInterfaceSizeClassCompact;
  trait_overrides.verticalSizeClass = UIUserInterfaceSizeClassCompact;
}

}  // namespace

class PrimaryToolbarViewControllerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    button_factory_ = ButtonFactory(ToolbarType::kPrimary);
    layout_guide_center_ = [[LayoutGuideCenter alloc] init];

    view_controller_ = [[PrimaryToolbarViewController alloc] init];
    view_controller_.buttonFactory = button_factory_;
    view_controller_.layoutGuideCenter = layout_guide_center_;
    view_ =
        base::apple::ObjCCastStrict<PrimaryToolbarView>(view_controller_.view);
    // The buttons only get the overridden size classes in a window.
    window_ = [[UIWindow alloc]
        initWithWindowScene:chrome_test_util::GetAnyWindowScene()];
    OverrideToCompactHeight(window_.traitOverrides);
    window_.rootViewController = view_controller_;
    window_.hidden = NO;
    [window_ layoutIfNeeded];
  }

  void TearDown() override {
    window_.hidden = YES;
    window_.rootViewController = nil;
    window_ = nil;
    PlatformTest::TearDown();
  }

  UIWindow* window_;
  PrimaryToolbarViewController* view_controller_;
  PrimaryToolbarView* view_;
  LegacyToolbarButtonFactory* button_factory_;
  LayoutGuideCenter* layout_guide_center_;
};

// Tests that the control buttons are hidden while a vertical sidebar shows
// them, and come back unchanged.
TEST_F(PrimaryToolbarViewControllerTest,
       HidesControlButtonsForVerticalSidebar) {
  NSArray<LegacyToolbarButton*>* buttons = view_.allButtons;
  ASSERT_EQ(UIUserInterfaceSizeClassCompact,
            view_.toolsMenuButton.traitCollection.verticalSizeClass);
  // Not split: the primary toolbar has the control buttons.
  ASSERT_FALSE(view_.toolsMenuButton.hidden);
  NSMutableArray<NSNumber*>* hidden_states = [NSMutableArray array];
  for (LegacyToolbarButton* button in buttons) {
    [hidden_states addObject:@(button.hidden)];
  }

  // The visible share button has its layout guide.
  ASSERT_EQ(view_.shareButton,
            [layout_guide_center_ referencedViewUnderName:kShareButtonGuide]);

  [view_controller_ setControlButtonsHiddenForVerticalSidebar:YES];
  for (LegacyToolbarButton* button in buttons) {
    EXPECT_TRUE(button.hidden);
  }
  // Hidden buttons don't keep their layout guides.
  EXPECT_FALSE(
      [layout_guide_center_ referencedViewUnderName:kShareButtonGuide]);
  // State and size class updates keep them hidden.
  ASSERT_TRUE(view_.stopButton.hiddenInCurrentState);
  view_.stopButton.hiddenInCurrentState = NO;
  [view_.toolsMenuButton updateHiddenInCurrentSizeClass];
  EXPECT_TRUE(view_.stopButton.hidden);
  EXPECT_TRUE(view_.toolsMenuButton.hidden);

  // The buttons come back as their states and size classes say, including the
  // stop button's state change made while they were hidden.
  [view_controller_ setControlButtonsHiddenForVerticalSidebar:NO];
  EXPECT_FALSE(view_.stopButton.hidden);
  for (NSUInteger i = 0; i < buttons.count; ++i) {
    if (buttons[i] != view_.stopButton) {
      EXPECT_EQ([hidden_states[i] boolValue], buttons[i].hidden);
    }
  }
  EXPECT_EQ(view_.shareButton,
            [layout_guide_center_ referencedViewUnderName:kShareButtonGuide]);
}

// Tests that the location bar spans the toolbar's width while the control
// buttons are hidden for the sidebar, when the toolbar isn't split, and sits
// between the buttons again afterwards.
TEST_F(PrimaryToolbarViewControllerTest,
       LocationBarSpansWidthForVerticalSidebar) {
  UIView* location_bar = view_.locationBarContainer;
  CGRect safe_area = view_.safeAreaLayoutGuide.layoutFrame;
  // Not split: the leading stack has the navigation buttons.
  ASSERT_GT(CGRectGetMinX(location_bar.frame),
            CGRectGetMinX(safe_area) + kExpandedLocationBarHorizontalMargin);

  [view_controller_ setControlButtonsHiddenForVerticalSidebar:YES];
  [window_ layoutIfNeeded];
  EXPECT_EQ(CGRectGetMinX(safe_area) + kExpandedLocationBarHorizontalMargin,
            CGRectGetMinX(location_bar.frame));
  EXPECT_EQ(CGRectGetMaxX(safe_area) - kExpandedLocationBarHorizontalMargin,
            CGRectGetMaxX(location_bar.frame));

  // Contracting the location bar after omnibox focus, which resets the split
  // toolbar mode, keeps it spanning the width.
  [view_controller_ expandLocationBar];
  [view_controller_ contractLocationBar];
  for (NSLayoutConstraint* constraint in view_.contractedNoMarginConstraints) {
    EXPECT_TRUE(constraint.active);
  }
  EXPECT_EQ(CGRectGetMinX(safe_area) + kExpandedLocationBarHorizontalMargin,
            CGRectGetMinX(location_bar.frame));

  [view_controller_ setControlButtonsHiddenForVerticalSidebar:NO];
  [window_ layoutIfNeeded];
  EXPECT_GT(CGRectGetMinX(location_bar.frame),
            CGRectGetMinX(safe_area) + kExpandedLocationBarHorizontalMargin);
}

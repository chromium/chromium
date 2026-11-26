// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/legacy/ui_bundled/secondary_toolbar_view_controller.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/toolbar/legacy/ui_bundled/buttons/legacy_toolbar_button.h"
#import "ios/chrome/browser/toolbar/legacy/ui_bundled/buttons/legacy_toolbar_button_factory.h"
#import "ios/chrome/browser/toolbar/legacy/ui_bundled/buttons/toolbar_button_visibility_configuration.h"
#import "ios/chrome/browser/toolbar/legacy/ui_bundled/buttons/toolbar_style.h"
#import "ios/chrome/browser/toolbar/legacy/ui_bundled/public/toolbar_type.h"
#import "ios/chrome/browser/toolbar/legacy/ui_bundled/secondary_toolbar_view.h"
#import "ios/chrome/browser/toolbar/ui/toolbar_height_delegate.h"
#import "ios/chrome/test/app/uikit_test_util.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

@interface SecondaryToolbarViewController (Testing)
@property(nonatomic) BOOL locationIndicatorActive;
- (void)collapsedToolbarButtonTapped;
- (void)showControlButtons;
- (void)hideControlButtons;
@end

class SecondaryToolbarViewControllerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    button_factory_ = [[LegacyToolbarButtonFactory alloc]
        initWithStyle:ToolbarStyle::kNormal];
    button_factory_.visibilityConfiguration =
        [[ToolbarButtonVisibilityConfiguration alloc]
            initWithType:ToolbarType::kSecondary];
    layout_guide_center_ = [[LayoutGuideCenter alloc] init];

    view_controller_ = [[SecondaryToolbarViewController alloc] init];
    view_controller_.buttonFactory = button_factory_;
    view_controller_.layoutGuideCenter = layout_guide_center_;

    // Force view loading to initialize subviews and constraints.
    view_ = base::apple::ObjCCastStrict<SecondaryToolbarView>(
        view_controller_.view);
  }

  void TearDown() override {
    window_.hidden = YES;
    window_.rootViewController = nil;
    window_ = nil;
    view_ = nil;
    view_controller_ = nil;
    button_factory_ = nil;
    layout_guide_center_ = nil;
    PlatformTest::TearDown();
  }

  // Window for the tests that need size classes.
  UIWindow* window_;
  SecondaryToolbarViewController* view_controller_;
  SecondaryToolbarView* view_;
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

// Tests that the button row is hidden and takes no height while the vertical
// sidebar shows the buttons, that hidden buttons release their layout guides,
// and that the row comes back unchanged.
TEST_F(SecondaryToolbarViewControllerTest, HidesButtonRowForVerticalSidebar) {
  // The buttons show in the split toolbar's size classes, which they only get
  // in a window.
  window_ = [[UIWindow alloc]
      initWithWindowScene:chrome_test_util::GetAnyWindowScene()];
  window_.traitOverrides.horizontalSizeClass = UIUserInterfaceSizeClassCompact;
  window_.traitOverrides.verticalSizeClass = UIUserInterfaceSizeClassRegular;
  window_.rootViewController = view_controller_;
  window_.hidden = NO;
  [window_ layoutIfNeeded];
  NSArray<LegacyToolbarButton*>* buttons = view_.allButtons;
  CGFloat button_row_height = view_.intrinsicContentSize.height;
  ASSERT_GT(button_row_height, 0);
  ASSERT_FALSE(view_.toolsMenuButton.hidden);
  ASSERT_EQ(view_.toolsMenuButton,
            [layout_guide_center_ referencedViewUnderName:kToolsMenuGuide]);

  [view_controller_ setControlButtonsHiddenForVerticalSidebar:YES];
  EXPECT_TRUE(view_.buttonStackView.hidden);
  EXPECT_EQ(0, view_.intrinsicContentSize.height);
  for (LegacyToolbarButton* button in buttons) {
    EXPECT_TRUE(button.hidden);
  }
  EXPECT_FALSE([layout_guide_center_ referencedViewUnderName:kToolsMenuGuide]);

  // Omnibox focus shows and hides the control buttons; the button row stays
  // hidden while the sidebar is active.
  [view_controller_ hideControlButtons];
  [view_controller_ showControlButtons];
  EXPECT_TRUE(view_.buttonStackView.hidden);

  // The control buttons stay hidden if omnibox focus hid them when the
  // sidebar goes away.
  [view_controller_ hideControlButtons];
  [view_controller_ setControlButtonsHiddenForVerticalSidebar:NO];
  EXPECT_TRUE(view_.buttonStackView.hidden);
  [view_controller_ showControlButtons];
  EXPECT_FALSE(view_.buttonStackView.hidden);
  EXPECT_EQ(button_row_height, view_.intrinsicContentSize.height);
  EXPECT_EQ(view_.toolsMenuButton,
            [layout_guide_center_ referencedViewUnderName:kToolsMenuGuide]);
}

// Tests that the toolbar height delegate is told when hiding the button row
// for the vertical sidebar changes the toolbar's height, and only then.
TEST_F(SecondaryToolbarViewControllerTest,
       NotifiesHeightChangeForVerticalSidebar) {
  id height_delegate = OCMStrictProtocolMock(@protocol(ToolbarHeightDelegate));
  view_controller_.toolbarHeightDelegate = height_delegate;

  OCMExpect([height_delegate toolbarsHeightChanged]);
  [view_controller_ setControlButtonsHiddenForVerticalSidebar:YES];
  EXPECT_OCMOCK_VERIFY(height_delegate);
  [view_controller_ setControlButtonsHiddenForVerticalSidebar:YES];

  OCMExpect([height_delegate toolbarsHeightChanged]);
  [view_controller_ setControlButtonsHiddenForVerticalSidebar:NO];
  EXPECT_OCMOCK_VERIFY(height_delegate);
  [view_controller_ setControlButtonsHiddenForVerticalSidebar:NO];
  EXPECT_OCMOCK_VERIFY(height_delegate);
}

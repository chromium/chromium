// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/ui/vertical_sidebar_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/public/commands/popup_menu_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/disabled_buttons_tap_gesture_recognizer.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button_actions.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button_factory.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_tab_grid_badge_button.h"
#import "ios/chrome/browser/toolbar/ui/toolbar_buttons_mutator.h"
#import "ios/chrome/browser/toolbar/ui/toolbar_constants.h"
#import "ios/chrome/test/app/uikit_test_util.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

// Size of the system vertical bar column on the iPhone Duo cover display in
// landscape.
constexpr CGFloat kColumnWidth = 84;
constexpr CGFloat kLandscapeColumnHeight = 466;

}  // namespace

// Fixture for testing `VerticalSidebarViewController`.
class VerticalSidebarViewControllerTest : public PlatformTest {
 protected:
  VerticalSidebarViewControllerTest() {
    mutator_ = OCMProtocolMock(@protocol(ToolbarButtonsMutator));
    popup_menu_handler_ = OCMProtocolMock(@protocol(PopupMenuCommands));
    // `view_controller_` keeps the actions.
    ToolbarButtonActions* actions = [[ToolbarButtonActions alloc] init];
    actions.mutator = mutator_;
    actions.popupMenuHandler = popup_menu_handler_;
    layout_guide_center_ = [[LayoutGuideCenter alloc] init];
    view_controller_ = [[VerticalSidebarViewController alloc]
        initWithButtonFactory:[[ToolbarButtonFactory alloc]
                                  initWithIncognito:NO]
                      actions:actions
            layoutGuideCenter:layout_guide_center_];
  }

  ~VerticalSidebarViewControllerTest() override {
    window_.hidden = YES;
    window_.rootViewController = nil;
  }

  // Shows the sidebar in a window with compact width and height (iPhone Duo
  // folded in landscape), where the toolbar hides most of its buttons.
  void ShowInCompactHeightWindow() {
    window_ = [[UIWindow alloc]
        initWithWindowScene:chrome_test_util::GetAnyWindowScene()];
    window_.traitOverrides.horizontalSizeClass =
        UIUserInterfaceSizeClassCompact;
    window_.traitOverrides.verticalSizeClass = UIUserInterfaceSizeClassCompact;
    window_.rootViewController = view_controller_;
    window_.hidden = NO;
    view_controller_.view.frame =
        CGRectMake(0, 0, kColumnWidth, kLandscapeColumnHeight);
    [window_ layoutIfNeeded];
  }

  // Returns the sidebar's button with `identifier`.
  ToolbarButton* Button(NSString* identifier) {
    return chrome_test_util::FindViewById<ToolbarButton>(view_controller_.view,
                                                         identifier);
  }

  // Returns the sidebar's recognizer of taps on disabled navigation buttons, or
  // nil if it has none.
  DisabledButtonsTapGestureRecognizer* DisabledNavigationTapRecognizer() {
    for (UIGestureRecognizer* recognizer in view_controller_.view
             .gestureRecognizers) {
      if (DisabledButtonsTapGestureRecognizer* disabled_buttons_recognizer =
              base::apple::ObjCCast<DisabledButtonsTapGestureRecognizer>(
                  recognizer)) {
        return disabled_buttons_recognizer;
      }
    }
    return nil;
  }

  id mutator_;
  id popup_menu_handler_;
  LayoutGuideCenter* layout_guide_center_;
  VerticalSidebarViewController* view_controller_;
  UIWindow* window_;
};

// Tests that the sidebar shows its five buttons, visible whatever the size
// class.
TEST_F(VerticalSidebarViewControllerTest, ShowsButtons) {
  ShowInCompactHeightWindow();
  EXPECT_NSEQ(kVerticalSidebarIdentifier,
              view_controller_.view.accessibilityIdentifier);
  for (NSString* identifier in @[
         kToolbarBackButtonIdentifier, kToolbarForwardButtonIdentifier,
         kToolbarOpenNewTabButtonIdentifier, kToolbarTabGridButtonIdentifier,
         kToolbarToolsMenuButtonIdentifier
       ]) {
    ToolbarButton* button = Button(identifier);
    ASSERT_TRUE(button) << identifier.UTF8String;
    EXPECT_FALSE(button.hidden) << identifier.UTF8String;
  }
}

// Tests that the consumer updates go to the sidebar's buttons.
TEST_F(VerticalSidebarViewControllerTest, AppliesConsumerUpdates) {
  [view_controller_ loadViewIfNeeded];
  ToolbarButton* back = Button(kToolbarBackButtonIdentifier);
  ToolbarButton* forward = Button(kToolbarForwardButtonIdentifier);
  ToolbarButton* new_tab = Button(kToolbarOpenNewTabButtonIdentifier);
  ToolbarTabGridBadgeButton* tab_grid =
      base::apple::ObjCCastStrict<ToolbarTabGridBadgeButton>(
          Button(kToolbarTabGridButtonIdentifier));
  UIMenu* back_menu = [UIMenu menuWithChildren:@[]];
  UIMenu* new_tab_menu = [UIMenu menuWithChildren:@[]];

  // Buttons start enabled, so the back button is disabled first to check that
  // `-setCanGoBack:` changes its state both ways.
  [view_controller_ setCanGoBack:NO];
  EXPECT_FALSE(back.enabled);

  [view_controller_ setCanGoBack:YES];
  [view_controller_ setCanGoForward:NO animated:YES];
  [view_controller_ setNewTabEnabled:NO];
  [view_controller_ updateTabCount:4];
  [view_controller_ setInTabGroup:YES];
  [view_controller_ setMenu:back_menu forButtonType:ToolbarButtonTypeBack];
  [view_controller_ setMenu:new_tab_menu forButtonType:ToolbarButtonTypeNewTab];

  EXPECT_TRUE(back.enabled);
  EXPECT_FALSE(forward.enabled);
  // A disabled forward button is still shown.
  EXPECT_FALSE(forward.hidden);
  EXPECT_FALSE(new_tab.enabled);
  EXPECT_EQ(4u, tab_grid.tabCount);
  EXPECT_TRUE(tab_grid.inTabGroup);
  // Buttons keep copies of their menus.
  EXPECT_NSEQ(back_menu.identifier, back.menu.identifier);
  EXPECT_NSEQ(new_tab_menu.identifier, new_tab.menu.identifier);
}

// Tests that the buttons' taps go through the actions, which have the mutator
// record their metrics.
TEST_F(VerticalSidebarViewControllerTest, TapsGoThroughActions) {
  [view_controller_ loadViewIfNeeded];

  OCMExpect([mutator_ recordUserActionsForButtonTapped:ToolbarButtonTypeBack]);
  OCMExpect([mutator_ goBack]);
  [Button(kToolbarBackButtonIdentifier)
      sendActionsForControlEvents:UIControlEventTouchUpInside];
  EXPECT_OCMOCK_VERIFY(mutator_);

  OCMExpect([mutator_ recordUserActionsForButtonTapped:ToolbarButtonTypeTools]);
  OCMExpect([popup_menu_handler_ showToolsMenuPopup]);
  [Button(kToolbarToolsMenuButtonIdentifier)
      sendActionsForControlEvents:UIControlEventTouchUpInside];
  EXPECT_OCMOCK_VERIFY(mutator_);
  EXPECT_OCMOCK_VERIFY(popup_menu_handler_);

  OCMExpect(
      [mutator_ recordUserActionsForButtonTapped:ToolbarButtonTypeNewTab]);
  OCMExpect([mutator_ openNewTabFromPoint:CGPointZero]).ignoringNonObjectArgs();
  [Button(kToolbarOpenNewTabButtonIdentifier)
      sendActionsForControlEvents:UIControlEventTouchUpInside];
  EXPECT_OCMOCK_VERIFY(mutator_);
}

// Tests that the buttons own the named layout guides while the sidebar is
// active, and release only the ones they own once it isn't.
TEST_F(VerticalSidebarViewControllerTest, ActiveSidebarOwnsGuides) {
  view_controller_.active = YES;
  EXPECT_EQ(Button(kToolbarBackButtonIdentifier),
            [layout_guide_center_ referencedViewUnderName:kBackButtonGuide]);
  EXPECT_EQ(Button(kToolbarForwardButtonIdentifier),
            [layout_guide_center_ referencedViewUnderName:kForwardButtonGuide]);
  EXPECT_EQ(Button(kToolbarTabGridButtonIdentifier),
            [layout_guide_center_ referencedViewUnderName:kTabSwitcherGuide]);
  EXPECT_EQ(Button(kToolbarToolsMenuButtonIdentifier),
            [layout_guide_center_ referencedViewUnderName:kToolsMenuGuide]);

  // Another view took a guide meanwhile.
  UIView* other_view = [[UIView alloc] init];
  [layout_guide_center_ referenceView:other_view underName:kToolsMenuGuide];

  view_controller_.active = NO;
  EXPECT_FALSE([layout_guide_center_ referencedViewUnderName:kBackButtonGuide]);
  EXPECT_FALSE(
      [layout_guide_center_ referencedViewUnderName:kForwardButtonGuide]);
  EXPECT_FALSE(
      [layout_guide_center_ referencedViewUnderName:kTabSwitcherGuide]);
  EXPECT_EQ(other_view,
            [layout_guide_center_ referencedViewUnderName:kToolsMenuGuide]);
}

// Tests the tools menu IPH highlight and blue dot, including the state set
// before the view is loaded.
TEST_F(VerticalSidebarViewControllerTest, PopupMenuUIUpdates) {
  [view_controller_ setOverflowMenuBlueDot:YES];
  [view_controller_ updateUIForOverflowMenuIPHDisplayed];
  ToolbarButton* tools_menu = Button(kToolbarToolsMenuButtonIdentifier);
  EXPECT_TRUE(tools_menu.hasBlueDot);
  EXPECT_TRUE(tools_menu.iphHighlighted);

  [view_controller_ updateUIForIPHDismissed];
  [view_controller_ setOverflowMenuBlueDot:NO];
  EXPECT_FALSE(tools_menu.iphHighlighted);
  EXPECT_FALSE(tools_menu.hasBlueDot);
}

// Tests that, as in the toolbar, the sidebar recognizes taps on its disabled
// navigation buttons only while the active tab is actor-controlled.
TEST_F(VerticalSidebarViewControllerTest, DisabledNavigationTapsWhileActor) {
  base::test::ScopedFeatureList feature_list(kActorTools);
  [view_controller_ loadViewIfNeeded];
  DisabledButtonsTapGestureRecognizer* recognizer =
      DisabledNavigationTapRecognizer();
  ASSERT_TRUE(recognizer);
  EXPECT_FALSE(recognizer.enabled);

  [view_controller_ setActorControlled:YES];
  EXPECT_TRUE(recognizer.enabled);

  [view_controller_ setActorControlled:NO];
  EXPECT_FALSE(recognizer.enabled);
}

// Tests that without the actor, the sidebar has no recognizer of taps on its
// disabled navigation buttons, and ignores the actor control state.
TEST_F(VerticalSidebarViewControllerTest,
       NoDisabledNavigationTapsWithoutActor) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kActorTools);
  [view_controller_ loadViewIfNeeded];
  EXPECT_FALSE(DisabledNavigationTapRecognizer());

  [view_controller_ setActorControlled:YES];
  EXPECT_FALSE(DisabledNavigationTapRecognizer());
}

// Tests that the buttons keep no shadow, the pills being their background, even
// in the old design after a style change.
TEST_F(VerticalSidebarViewControllerTest, ButtonsHaveNoShadowAfterStyleChange) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({kChromeNextIa, kNextOldDesign}, {});
  ShowInCompactHeightWindow();
  for (NSString* identifier in @[
         kToolbarBackButtonIdentifier, kToolbarForwardButtonIdentifier,
         kToolbarOpenNewTabButtonIdentifier, kToolbarTabGridButtonIdentifier,
         kToolbarToolsMenuButtonIdentifier
       ]) {
    ToolbarButton* button = Button(identifier);
    ASSERT_TRUE(button) << identifier.UTF8String;
    // The buttons only re-apply their shadow from their style change callback.
    // A style change on the window doesn't reliably reach them synchronously,
    // so the style is changed on each button itself.
    button.overrideUserInterfaceStyle = UIUserInterfaceStyleLight;
    [button updateTraitsIfNeeded];
    button.overrideUserInterfaceStyle = UIUserInterfaceStyleDark;
    [button updateTraitsIfNeeded];
    EXPECT_EQ(UIUserInterfaceStyleDark,
              button.traitCollection.userInterfaceStyle)
        << identifier.UTF8String;
    EXPECT_EQ(0, button.layer.shadowOpacity) << identifier.UTF8String;
  }
}

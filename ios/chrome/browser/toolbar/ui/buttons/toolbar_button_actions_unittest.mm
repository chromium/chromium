// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button_actions.h"

#import "base/test/metrics/user_action_tester.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/shared/public/commands/activity_service_commands.h"
#import "ios/chrome/browser/shared/public/commands/actor_overlay_commands.h"
#import "ios/chrome/browser/shared/public/commands/popup_menu_commands.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/ui/util/disabled_buttons_tap_gesture_recognizer.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button_factory.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_tab_grid_badge_button.h"
#import "ios/chrome/browser/toolbar/ui/toolbar_buttons_mutator.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

@interface ToolbarButtonActions (Testing)
// Returns the items of the tab grid buttons' menu, as its deferred element
// does when the menu opens.
- (NSArray<UIMenuElement*>*)tabGridMenuElements;
// Handles taps on disabled navigation buttons, as the recognizer returned by
// `-disabledNavigationTapRecognizerForButtons:` does.
- (void)disabledNavigationButtonTapped;
@end

// Fixture for testing ToolbarButtonActions.
class ToolbarButtonActionsTest : public PlatformTest {
 protected:
  ToolbarButtonActionsTest() {
    mutator_ = OCMStrictProtocolMock(@protocol(ToolbarButtonsMutator));
    [mutator_ setExpectationOrderMatters:YES];
    popup_menu_handler_ = OCMProtocolMock(@protocol(PopupMenuCommands));
    activity_service_handler_ =
        OCMProtocolMock(@protocol(ActivityServiceCommands));
    scene_handler_ = OCMProtocolMock(@protocol(SceneCommands));

    actions_ = [[ToolbarButtonActions alloc] init];
    actions_.mutator = mutator_;
    actions_.popupMenuHandler = popup_menu_handler_;
    actions_.activityServiceHandler = activity_service_handler_;
    actions_.sceneHandler = scene_handler_;

    factory_ = [[ToolbarButtonFactory alloc] initWithIncognito:NO];
  }

  base::test::TaskEnvironment task_environment_;
  id mutator_;
  id popup_menu_handler_;
  id activity_service_handler_;
  id scene_handler_;
  ToolbarButtonActions* actions_;
  ToolbarButtonFactory* factory_;
};

// Tests that the back button records its user actions, then goes back.
TEST_F(ToolbarButtonActionsTest, BackTapRecordsActionsThenGoesBack) {
  ToolbarButton* button = [factory_ makeBackButton];
  [actions_ configureButton:button ofType:ToolbarButtonTypeBack];
  OCMExpect([mutator_ recordUserActionsForButtonTapped:ToolbarButtonTypeBack]);
  OCMExpect([mutator_ goBack]);
  [button sendActionsForControlEvents:UIControlEventTouchUpInside];
  EXPECT_OCMOCK_VERIFY(mutator_);
}

// Tests that the forward button records its user actions, then goes forward.
TEST_F(ToolbarButtonActionsTest, ForwardTapRecordsActionsThenGoesForward) {
  ToolbarButton* button = [factory_ makeForwardButton];
  [actions_ configureButton:button ofType:ToolbarButtonTypeForward];
  OCMExpect(
      [mutator_ recordUserActionsForButtonTapped:ToolbarButtonTypeForward]);
  OCMExpect([mutator_ goForward]);
  [button sendActionsForControlEvents:UIControlEventTouchUpInside];
  EXPECT_OCMOCK_VERIFY(mutator_);
}

// Tests that the back and forward buttons' menus record their user action.
TEST_F(ToolbarButtonActionsTest, NavigationMenusRecordMenuTriggered) {
  ToolbarButton* back_button = [factory_ makeBackButton];
  [actions_ configureButton:back_button ofType:ToolbarButtonTypeBack];
  ToolbarButton* forward_button = [factory_ makeForwardButton];
  [actions_ configureButton:forward_button ofType:ToolbarButtonTypeForward];

  base::UserActionTester user_action_tester;
  [back_button sendActionsForControlEvents:UIControlEventMenuActionTriggered];
  [forward_button
      sendActionsForControlEvents:UIControlEventMenuActionTriggered];
  EXPECT_EQ(
      2, user_action_tester.GetActionCount("MobileMenuToolbarMenuTriggered"));
}

// Tests that the reload button records its user actions, then reloads.
TEST_F(ToolbarButtonActionsTest, ReloadTapRecordsActionsThenReloads) {
  ToolbarButton* button = [factory_ makeReloadButton];
  [actions_ configureButton:button ofType:ToolbarButtonTypeReload];
  OCMExpect(
      [mutator_ recordUserActionsForButtonTapped:ToolbarButtonTypeReload]);
  OCMExpect([mutator_ reload]);
  [button sendActionsForControlEvents:UIControlEventTouchUpInside];
  EXPECT_OCMOCK_VERIFY(mutator_);
}

// Tests that the stop button records its user actions, then stops loading.
TEST_F(ToolbarButtonActionsTest, StopTapRecordsActionsThenStops) {
  ToolbarButton* button = [factory_ makeStopButton];
  [actions_ configureButton:button ofType:ToolbarButtonTypeStop];
  OCMExpect([mutator_ recordUserActionsForButtonTapped:ToolbarButtonTypeStop]);
  OCMExpect([mutator_ stop]);
  [button sendActionsForControlEvents:UIControlEventTouchUpInside];
  EXPECT_OCMOCK_VERIFY(mutator_);
}

// Tests that the share button records its user actions and shows the share
// sheet from the button.
TEST_F(ToolbarButtonActionsTest, ShareTapRecordsActionsThenShowsShareSheet) {
  ToolbarButton* button = [factory_ makeShareButton];
  [actions_ configureButton:button ofType:ToolbarButtonTypeShare];
  OCMExpect([mutator_ recordUserActionsForButtonTapped:ToolbarButtonTypeShare]);
  OCMExpect([activity_service_handler_ showShareSheetFromShareButton:button]);
  [button sendActionsForControlEvents:UIControlEventTouchUpInside];
  EXPECT_OCMOCK_VERIFY(mutator_);
  EXPECT_OCMOCK_VERIFY(activity_service_handler_);
}

// Tests that the assistant button records its user actions, then notifies the
// mutator.
TEST_F(ToolbarButtonActionsTest,
       AssistantTapRecordsActionsThenNotifiesMutator) {
  ToolbarButton* button = [factory_ makeAssistantButton];
  [actions_ configureButton:button ofType:ToolbarButtonTypeAssistant];
  OCMExpect(
      [mutator_ recordUserActionsForButtonTapped:ToolbarButtonTypeAssistant]);
  OCMExpect([mutator_ assistantButtonTapped]);
  [button sendActionsForControlEvents:UIControlEventTouchUpInside];
  EXPECT_OCMOCK_VERIFY(mutator_);
}

// Tests that the tools menu button records its user actions and shows the tools
// menu.
TEST_F(ToolbarButtonActionsTest, ToolsMenuTapRecordsActionsThenShowsToolsMenu) {
  ToolbarButton* button = [factory_ makeToolsMenuButton];
  [actions_ configureButton:button ofType:ToolbarButtonTypeTools];
  OCMExpect([mutator_ recordUserActionsForButtonTapped:ToolbarButtonTypeTools]);
  OCMExpect([popup_menu_handler_ showToolsMenuPopup]);
  [button sendActionsForControlEvents:UIControlEventTouchUpInside];
  EXPECT_OCMOCK_VERIFY(mutator_);
  EXPECT_OCMOCK_VERIFY(popup_menu_handler_);
}

// Tests that the tab grid button prepares the tab switcher on touch down.
TEST_F(ToolbarButtonActionsTest, TabGridTouchDownPreparesTabSwitcher) {
  ToolbarTabGridBadgeButton* button = [factory_ makeTabGridButton];
  [actions_ configureButton:button ofType:ToolbarButtonTypeTabGrid];

  OCMExpect([scene_handler_ prepareTabSwitcher]);
  [button sendActionsForControlEvents:UIControlEventTouchDown];
  EXPECT_OCMOCK_VERIFY(scene_handler_);
}

// Tests that the tab grid button records its user actions, then shows the tab
// grid, on touch up.
TEST_F(ToolbarButtonActionsTest, TabGridTapRecordsActionsThenShowsTabGrid) {
  ToolbarTabGridBadgeButton* button = [factory_ makeTabGridButton];
  [actions_ configureButton:button ofType:ToolbarButtonTypeTabGrid];

  OCMExpect(
      [mutator_ recordUserActionsForButtonTapped:ToolbarButtonTypeTabGrid]);
  OCMExpect([scene_handler_ displayTabGridInMode:TabGridOpeningMode::kDefault]);
  [button sendActionsForControlEvents:UIControlEventTouchUpInside];
  EXPECT_OCMOCK_VERIFY(mutator_);
  EXPECT_OCMOCK_VERIFY(scene_handler_);
}

// Tests that the tab grid button's menu, built when it opens, shows the items
// of the latest tab grid menu and records that it's shown.
TEST_F(ToolbarButtonActionsTest, TabGridMenuShowsLatestMenuAndRecordsIt) {
  ToolbarTabGridBadgeButton* button = [factory_ makeTabGridButton];
  [actions_ configureButton:button ofType:ToolbarButtonTypeTabGrid];
  EXPECT_FALSE(button.showsMenuAsPrimaryAction);
  ASSERT_EQ(1u, button.menu.children.count);
  EXPECT_TRUE([button.menu.children.firstObject
      isKindOfClass:[UIDeferredMenuElement class]]);

  UIAction* old_action = [UIAction actionWithHandler:^(UIAction*){
  }];
  UIAction* new_action = [UIAction actionWithHandler:^(UIAction*){
  }];
  [actions_ setTabGridMenu:[UIMenu menuWithChildren:@[ old_action ]]];
  [actions_ setTabGridMenu:[UIMenu menuWithChildren:@[ new_action ]]];
  base::UserActionTester user_action_tester;
  EXPECT_NSEQ(@[ new_action ], [actions_ tabGridMenuElements]);
  EXPECT_EQ(
      1, user_action_tester.GetActionCount("MobileMenuToolbarMenuTriggered"));
}

// Tests that the tab grid button's menu has no items, and isn't recorded, while
// there's no tab grid menu or an empty one, as UIKit then shows no menu.
TEST_F(ToolbarButtonActionsTest, TabGridMenuIsEmptyWithoutMenuItems) {
  ToolbarTabGridBadgeButton* button = [factory_ makeTabGridButton];
  [actions_ configureButton:button ofType:ToolbarButtonTypeTabGrid];
  base::UserActionTester user_action_tester;
  EXPECT_NSEQ(@[], [actions_ tabGridMenuElements]);

  [actions_ setTabGridMenu:[UIMenu menuWithChildren:@[]]];
  EXPECT_NSEQ(@[], [actions_ tabGridMenuElements]);

  [actions_ setTabGridMenu:nil];
  EXPECT_NSEQ(@[], [actions_ tabGridMenuElements]);
  EXPECT_EQ(
      0, user_action_tester.GetActionCount("MobileMenuToolbarMenuTriggered"));
}

// Tests that the disabled navigation buttons' tap recognizer shows that the
// actor limits interactions with the tab.
TEST_F(ToolbarButtonActionsTest,
       DisabledNavigationTapShowsActorInteractionLimitedSnackbar) {
  id actor_overlay_handler =
      OCMStrictProtocolMock(@protocol(ActorOverlayCommands));
  actions_.actorOverlayHandler = actor_overlay_handler;
  ToolbarButton* back_button = [factory_ makeBackButton];
  UIGestureRecognizer* recognizer =
      [actions_ disabledNavigationTapRecognizerForButtons:@[ back_button ]];
  EXPECT_TRUE(
      [recognizer isKindOfClass:[DisabledButtonsTapGestureRecognizer class]]);

  OCMExpect([actor_overlay_handler showActorInteractionLimitedSnackbar]);
  [actions_ disabledNavigationButtonTapped];
  EXPECT_OCMOCK_VERIFY(actor_overlay_handler);
}

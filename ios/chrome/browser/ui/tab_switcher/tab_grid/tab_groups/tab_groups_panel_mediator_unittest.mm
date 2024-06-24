// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_groups_panel_mediator.h"

#import "base/test/metrics/user_action_tester.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_toolbars_configuration.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/test/fake_tab_grid_toolbars_mediator.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/platform_test.h"

namespace {
const char* kSelectTabGroupsUMA = "MobileTabGridSelectTabGroups";
}  // namespace

class TabGroupsPanelMediatorTest : public PlatformTest {
 protected:
  TabGroupsPanelMediatorTest() : web_state_list_(&web_state_list_delegate_) {}

  FakeWebStateListDelegate web_state_list_delegate_;
  WebStateList web_state_list_;
};

// Tests that the UMA for selecting the Tab Groups panel is correctly recorded.
TEST_F(TabGroupsPanelMediatorTest, RecordUMAWhenSelected) {
  base::UserActionTester user_action_tester;
  TabGroupsPanelMediator* mediator = [[TabGroupsPanelMediator alloc]
      initWithRegularWebStateList:&web_state_list_
                 disabledByPolicy:NO];

  EXPECT_EQ(0, user_action_tester.GetActionCount(kSelectTabGroupsUMA));

  // Select a different page.
  [mediator currentlySelectedGrid:NO];
  EXPECT_EQ(0, user_action_tester.GetActionCount(kSelectTabGroupsUMA));

  // Select Tab Groups.
  [mediator currentlySelectedGrid:YES];
  EXPECT_EQ(1, user_action_tester.GetActionCount(kSelectTabGroupsUMA));

  // Unselect Tab Groups.
  [mediator currentlySelectedGrid:NO];
  EXPECT_EQ(1, user_action_tester.GetActionCount(kSelectTabGroupsUMA));
}

// Tests that when the panel not the selected one, no toolbar delegate is set,
// no toolbar config is returned.
TEST_F(TabGroupsPanelMediatorTest, NotSelected_NoToolbarsDelegateOrConfig) {
  TabGroupsPanelMediator* mediator = [[TabGroupsPanelMediator alloc]
      initWithRegularWebStateList:&web_state_list_
                 disabledByPolicy:NO];
  FakeTabGridToolbarsMediator* toolbars_mutator =
      [[FakeTabGridToolbarsMediator alloc] init];
  mediator.toolbarsMutator = toolbars_mutator;

  [mediator currentlySelectedGrid:NO];

  EXPECT_EQ(toolbars_mutator.delegate, nil);
  EXPECT_EQ(toolbars_mutator.configuration, nil);
}

// Tests that when the panel is disabled by policy, the toolbars config is the
// disabled one.
TEST_F(TabGroupsPanelMediatorTest, DisabledByPolicy_DisabledToolbarsConfig) {
  TabGroupsPanelMediator* mediator = [[TabGroupsPanelMediator alloc]
      initWithRegularWebStateList:&web_state_list_
                 disabledByPolicy:YES];
  FakeTabGridToolbarsMediator* toolbars_mutator =
      [[FakeTabGridToolbarsMediator alloc] init];
  mediator.toolbarsMutator = toolbars_mutator;

  [mediator currentlySelectedGrid:YES];

  EXPECT_EQ(toolbars_mutator.delegate,
            static_cast<id<TabGridToolbarsGridDelegate>>(mediator));
  EXPECT_NE(toolbars_mutator.configuration, nil);
  EXPECT_EQ(TabGridPageTabGroups, toolbars_mutator.configuration.page);
  EXPECT_EQ(TabGridModeNormal, toolbars_mutator.configuration.mode);

  // All buttons are disabled.
  EXPECT_FALSE(toolbars_mutator.configuration.doneButton);

  EXPECT_FALSE(toolbars_mutator.configuration.selectAllButton);
  EXPECT_EQ(0u, toolbars_mutator.configuration.selectedItemsCount);
  EXPECT_FALSE(toolbars_mutator.configuration.closeSelectedTabsButton);
  EXPECT_FALSE(toolbars_mutator.configuration.shareButton);
  EXPECT_FALSE(toolbars_mutator.configuration.addToButton);

  EXPECT_FALSE(toolbars_mutator.configuration.closeAllButton);
  EXPECT_FALSE(toolbars_mutator.configuration.newTabButton);
  EXPECT_FALSE(toolbars_mutator.configuration.searchButton);
  EXPECT_FALSE(toolbars_mutator.configuration.selectTabsButton);
  EXPECT_FALSE(toolbars_mutator.configuration.undoButton);
  EXPECT_FALSE(toolbars_mutator.configuration.deselectAllButton);
  EXPECT_FALSE(toolbars_mutator.configuration.cancelSearchButton);
}

// Tests that when the panel is selected and not disabled by policy, the
// toolbars delegate and config are set accordingly. Since there are no tab in
// the Regular Tabs page, the Done button is still disabled.
TEST_F(TabGroupsPanelMediatorTest,
       EnabledByPolicyAndSelectedButNoRegularTab_DoneButtonDisabled) {
  TabGroupsPanelMediator* mediator = [[TabGroupsPanelMediator alloc]
      initWithRegularWebStateList:&web_state_list_
                 disabledByPolicy:NO];
  FakeTabGridToolbarsMediator* toolbars_mutator =
      [[FakeTabGridToolbarsMediator alloc] init];
  mediator.toolbarsMutator = toolbars_mutator;

  [mediator currentlySelectedGrid:YES];

  EXPECT_EQ(toolbars_mutator.delegate,
            static_cast<id<TabGridToolbarsGridDelegate>>(mediator));
  EXPECT_NE(toolbars_mutator.configuration, nil);
  EXPECT_EQ(TabGridPageTabGroups, toolbars_mutator.configuration.page);
  EXPECT_EQ(TabGridModeNormal, toolbars_mutator.configuration.mode);

  // Done button is disabled.
  EXPECT_FALSE(toolbars_mutator.configuration.doneButton);

  // All other buttons are disabled.
  EXPECT_FALSE(toolbars_mutator.configuration.selectAllButton);
  EXPECT_EQ(0u, toolbars_mutator.configuration.selectedItemsCount);
  EXPECT_FALSE(toolbars_mutator.configuration.closeSelectedTabsButton);
  EXPECT_FALSE(toolbars_mutator.configuration.shareButton);
  EXPECT_FALSE(toolbars_mutator.configuration.addToButton);

  EXPECT_FALSE(toolbars_mutator.configuration.closeAllButton);
  EXPECT_FALSE(toolbars_mutator.configuration.newTabButton);
  EXPECT_FALSE(toolbars_mutator.configuration.searchButton);
  EXPECT_FALSE(toolbars_mutator.configuration.selectTabsButton);
  EXPECT_FALSE(toolbars_mutator.configuration.undoButton);
  EXPECT_FALSE(toolbars_mutator.configuration.deselectAllButton);
  EXPECT_FALSE(toolbars_mutator.configuration.cancelSearchButton);
}

// Tests that when the panel is selected and not disabled by policy, the
// toolbars delegate and config are set accordingly. Since there is a Regular
// tab, the Done button is finally enabled.
TEST_F(TabGroupsPanelMediatorTest,
       EnabledByPolicyAndSelectedWithRegularTab_DoneButtonEnabled) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state_list_.InsertWebState(std::move(web_state));
  TabGroupsPanelMediator* mediator = [[TabGroupsPanelMediator alloc]
      initWithRegularWebStateList:&web_state_list_
                 disabledByPolicy:NO];
  FakeTabGridToolbarsMediator* toolbars_mutator =
      [[FakeTabGridToolbarsMediator alloc] init];
  mediator.toolbarsMutator = toolbars_mutator;

  [mediator currentlySelectedGrid:YES];

  EXPECT_EQ(toolbars_mutator.delegate,
            static_cast<id<TabGridToolbarsGridDelegate>>(mediator));
  EXPECT_NE(toolbars_mutator.configuration, nil);
  EXPECT_EQ(TabGridPageTabGroups, toolbars_mutator.configuration.page);
  EXPECT_EQ(TabGridModeNormal, toolbars_mutator.configuration.mode);

  // Done button is enabled.
  EXPECT_TRUE(toolbars_mutator.configuration.doneButton);

  // All other buttons are disabled.
  EXPECT_FALSE(toolbars_mutator.configuration.selectAllButton);
  EXPECT_EQ(0u, toolbars_mutator.configuration.selectedItemsCount);
  EXPECT_FALSE(toolbars_mutator.configuration.closeSelectedTabsButton);
  EXPECT_FALSE(toolbars_mutator.configuration.shareButton);
  EXPECT_FALSE(toolbars_mutator.configuration.addToButton);

  EXPECT_FALSE(toolbars_mutator.configuration.closeAllButton);
  EXPECT_FALSE(toolbars_mutator.configuration.newTabButton);
  EXPECT_FALSE(toolbars_mutator.configuration.searchButton);
  EXPECT_FALSE(toolbars_mutator.configuration.selectTabsButton);
  EXPECT_FALSE(toolbars_mutator.configuration.undoButton);
  EXPECT_FALSE(toolbars_mutator.configuration.deselectAllButton);
  EXPECT_FALSE(toolbars_mutator.configuration.cancelSearchButton);
}

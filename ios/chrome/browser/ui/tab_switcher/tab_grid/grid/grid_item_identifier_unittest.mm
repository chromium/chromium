// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_item_identifier.h"

#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/web_state_list_builder_from_description.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_group_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_item.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state_id.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

class GridItemIdentifierTest : public PlatformTest {
 public:
  GridItemIdentifierTest() {
    TestProfileIOS::Builder profile_builder;
    profile_ = std::move(profile_builder).Build();
    browser_ = std::make_unique<TestBrowser>(
        profile_.get(), std::make_unique<FakeWebStateListDelegate>());
    web_state_list_ = browser_->GetWebStateList();
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  raw_ptr<WebStateList> web_state_list_;
};

// Verifies the properties of the InactiveTabsButton grid item.
TEST_F(GridItemIdentifierTest, InactiveTabsButton) {
  GridItemIdentifier* item_identifier =
      [GridItemIdentifier inactiveTabsButtonIdentifier];

  EXPECT_EQ(item_identifier.type, GridItemType::kInactiveTabsButton);
  EXPECT_NSEQ(item_identifier.tabSwitcherItem, nil);
  EXPECT_NSEQ(item_identifier.tabGroupItem, nil);
}

// Verifies the properties of a Tab grid item.
TEST_F(GridItemIdentifierTest, Tab) {
  web::WebStateID web_state_id = web::WebStateID::NewUnique();
  TabSwitcherItem* tab_switcher_item =
      [[TabSwitcherItem alloc] initWithIdentifier:web_state_id];
  GridItemIdentifier* item_identifier =
      [[GridItemIdentifier alloc] initWithTabItem:tab_switcher_item];

  EXPECT_EQ(item_identifier.type, GridItemType::kTab);
  EXPECT_NSEQ(item_identifier.tabSwitcherItem, tab_switcher_item);
  EXPECT_NSEQ(item_identifier.tabGroupItem, nil);
}

// Verifies the properties of a Group grid item.
TEST_F(GridItemIdentifierTest, Group) {
  WebStateListBuilderFromDescription builder(web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| [ 0 b c ] d"));

  const TabGroup* group = web_state_list_->GetGroupOfWebStateAt(0);
  ASSERT_TRUE(group);
  TabGroupItem* tab_group_item =
      [[TabGroupItem alloc] initWithTabGroup:group
                                webStateList:web_state_list_];
  GridItemIdentifier* item_identifier =
      [[GridItemIdentifier alloc] initWithGroupItem:tab_group_item];

  EXPECT_EQ(item_identifier.type, GridItemType::kGroup);
  EXPECT_NSEQ(item_identifier.tabSwitcherItem, nil);
  EXPECT_NSEQ(item_identifier.tabGroupItem, tab_group_item);
}

// Verifies the properties of the SuggestedActions grid item.
TEST_F(GridItemIdentifierTest, SuggestedActions) {
  GridItemIdentifier* item_identifier =
      [GridItemIdentifier suggestedActionsIdentifier];

  EXPECT_EQ(item_identifier.type, GridItemType::kSuggestedActions);
  EXPECT_NSEQ(item_identifier.tabSwitcherItem, nil);
  EXPECT_NSEQ(item_identifier.tabGroupItem, nil);
}

// Verifies the equality of Tab grid items.
TEST_F(GridItemIdentifierTest, TabEqualTab) {
  web::WebStateID web_state_id = web::WebStateID::NewUnique();
  TabSwitcherItem* tab_switcher_item_1 =
      [[TabSwitcherItem alloc] initWithIdentifier:web_state_id];
  GridItemIdentifier* item_identifier_1 =
      [[GridItemIdentifier alloc] initWithTabItem:tab_switcher_item_1];
  TabSwitcherItem* tab_switcher_item_2 =
      [[TabSwitcherItem alloc] initWithIdentifier:web_state_id];
  GridItemIdentifier* item_identifier_2 =
      [[GridItemIdentifier alloc] initWithTabItem:tab_switcher_item_2];

  EXPECT_NSEQ(item_identifier_1, item_identifier_2);
  EXPECT_EQ(item_identifier_1.hash, item_identifier_2.hash);
}

// Verifies the equality of Group grid items.
TEST_F(GridItemIdentifierTest, GroupEqualGroup) {
  WebStateListBuilderFromDescription builder(web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| [ 0 b c ] d"));

  const TabGroup* group = web_state_list_->GetGroupOfWebStateAt(0);
  ASSERT_TRUE(group);

  TabGroupItem* tab_group_item_1 =
      [[TabGroupItem alloc] initWithTabGroup:group
                                webStateList:web_state_list_];
  GridItemIdentifier* item_identifier_1 =
      [[GridItemIdentifier alloc] initWithGroupItem:tab_group_item_1];

  TabGroupItem* tab_group_item_2 =
      [[TabGroupItem alloc] initWithTabGroup:group
                                webStateList:web_state_list_];
  GridItemIdentifier* item_identifier_2 =
      [[GridItemIdentifier alloc] initWithGroupItem:tab_group_item_2];

  EXPECT_NSEQ(item_identifier_1, item_identifier_2);
  EXPECT_EQ(item_identifier_1.hash, item_identifier_2.hash);
}

// Verifies the inequality of Tab grid items.
TEST_F(GridItemIdentifierTest, TabNotEqualTab) {
  web::WebStateID web_state_id_1 = web::WebStateID::FromSerializedValue(42);
  TabSwitcherItem* tab_switcher_item_1 =
      [[TabSwitcherItem alloc] initWithIdentifier:web_state_id_1];
  GridItemIdentifier* item_identifier_1 =
      [[GridItemIdentifier alloc] initWithTabItem:tab_switcher_item_1];
  web::WebStateID web_state_id_2 = web::WebStateID::FromSerializedValue(43);
  TabSwitcherItem* tab_switcher_item_2 =
      [[TabSwitcherItem alloc] initWithIdentifier:web_state_id_2];
  GridItemIdentifier* item_identifier_2 =
      [[GridItemIdentifier alloc] initWithTabItem:tab_switcher_item_2];

  EXPECT_NSNE(item_identifier_1, item_identifier_2);
  EXPECT_NE(item_identifier_1.hash, item_identifier_2.hash);
  // Check also that hashes are not consecutive.
  EXPECT_NE(item_identifier_1.hash + 1, item_identifier_2.hash);
}

// Verifies the inequality of Group grid items.
TEST_F(GridItemIdentifierTest, GroupNotEqualGroup) {
  WebStateListBuilderFromDescription builder(web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| [ 0 b ] [ 1 c ] d"));

  const TabGroup* group_1 = web_state_list_->GetGroupOfWebStateAt(0);
  ASSERT_TRUE(group_1);
  TabGroupItem* tab_group_item_1 =
      [[TabGroupItem alloc] initWithTabGroup:group_1
                                webStateList:web_state_list_];

  GridItemIdentifier* item_identifier_1 =
      [[GridItemIdentifier alloc] initWithGroupItem:tab_group_item_1];

  const TabGroup* group_2 = web_state_list_->GetGroupOfWebStateAt(1);
  ASSERT_TRUE(group_2);
  TabGroupItem* tab_group_item_2 =
      [[TabGroupItem alloc] initWithTabGroup:group_2
                                webStateList:web_state_list_];
  GridItemIdentifier* item_identifier_2 =
      [[GridItemIdentifier alloc] initWithGroupItem:tab_group_item_2];

  EXPECT_NSNE(item_identifier_1, item_identifier_2);
  EXPECT_NE(item_identifier_1.hash, item_identifier_2.hash);
  // Check also that hashes are not consecutive.
  EXPECT_NE(item_identifier_1.hash + 1, item_identifier_2.hash);
}

// Verifies the inequality of a Tab and a SuggestedActions grid items.
TEST_F(GridItemIdentifierTest, TabNotEqualSuggestedAction) {
  web::WebStateID web_state_id = web::WebStateID::NewUnique();
  TabSwitcherItem* tab_switcher_item =
      [[TabSwitcherItem alloc] initWithIdentifier:web_state_id];
  GridItemIdentifier* tab_item_identifier =
      [[GridItemIdentifier alloc] initWithTabItem:tab_switcher_item];

  GridItemIdentifier* suggested_actions_item_identifier =
      [GridItemIdentifier suggestedActionsIdentifier];
  GridItemIdentifier* inactive_tabs_item_identifier =
      [GridItemIdentifier inactiveTabsButtonIdentifier];

  EXPECT_NSNE(tab_item_identifier, suggested_actions_item_identifier);
  EXPECT_NSNE(tab_item_identifier, inactive_tabs_item_identifier);
}

// Verifies the inequality of a Group and a SuggestedActions grid items.
TEST_F(GridItemIdentifierTest, GroupNotEqualSuggestedAction) {
  WebStateListBuilderFromDescription builder(web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| [ 0 b c ] d"));

  const TabGroup* group = web_state_list_->GetGroupOfWebStateAt(0);
  ASSERT_TRUE(group);
  TabGroupItem* tab_group_item =
      [[TabGroupItem alloc] initWithTabGroup:group
                                webStateList:web_state_list_];
  GridItemIdentifier* group_item_identifier =
      [[GridItemIdentifier alloc] initWithGroupItem:tab_group_item];

  GridItemIdentifier* suggested_actions_item_identifier =
      [GridItemIdentifier suggestedActionsIdentifier];
  GridItemIdentifier* inactive_tabs_item_identifier =
      [GridItemIdentifier inactiveTabsButtonIdentifier];

  EXPECT_NSNE(group_item_identifier, suggested_actions_item_identifier);
  EXPECT_NSNE(group_item_identifier, inactive_tabs_item_identifier);
}

// Verifies the equality of SuggestedActions grid items.
TEST_F(GridItemIdentifierTest, SuggestedActionsEqualSuggestedActions) {
  GridItemIdentifier* item_identifier_1 =
      [GridItemIdentifier suggestedActionsIdentifier];
  GridItemIdentifier* item_identifier_2 =
      [GridItemIdentifier suggestedActionsIdentifier];

  EXPECT_NSEQ(item_identifier_1, item_identifier_2);
  EXPECT_EQ(item_identifier_1.hash, item_identifier_2.hash);
}

// Verifies the equality of InactiveTabsButton grid items.
TEST_F(GridItemIdentifierTest, InactiveTabsEqualInactiveTabs) {
  GridItemIdentifier* item_identifier_1 =
      [GridItemIdentifier inactiveTabsButtonIdentifier];
  GridItemIdentifier* item_identifier_2 =
      [GridItemIdentifier inactiveTabsButtonIdentifier];

  EXPECT_NSEQ(item_identifier_1, item_identifier_2);
  EXPECT_EQ(item_identifier_1.hash, item_identifier_2.hash);
}

// Checks that GridItemIdentifier's hashing follows the current NSNumber
// implementation.
TEST_F(GridItemIdentifierTest, HashingLikeNSNumber) {
  std::vector<web::WebStateID> web_state_ids = {
      web::WebStateID::FromSerializedValue(1),
      web::WebStateID::FromSerializedValue(42),
      web::WebStateID::FromSerializedValue(816831566),
      web::WebStateID::FromSerializedValue(816834219),
      web::WebStateID::NewUnique(),
  };

  for (auto web_state_id : web_state_ids) {
    TabSwitcherItem* tab_switcher_item =
        [[TabSwitcherItem alloc] initWithIdentifier:web_state_id];
    GridItemIdentifier* tab_item_identifier =
        [[GridItemIdentifier alloc] initWithTabItem:tab_switcher_item];

    EXPECT_EQ(tab_item_identifier.hash, @(web_state_id.identifier()).hash);
  }
}

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/tab_groups/tab_group_id.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_group_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_strip/ui/swift.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_item.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/web_state_id.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using TabStripItemIdentifierTest = PlatformTest;

using tab_groups::TabGroupId;

// Verifies the properties of a Tab strip tab item.
TEST_F(TabStripItemIdentifierTest, Tab) {
  web::WebStateID web_state_id = web::WebStateID::NewUnique();
  TabSwitcherItem* tab_switcher_item =
      [[TabSwitcherItem alloc] initWithIdentifier:web_state_id];
  TabStripItemIdentifier* item_identifier =
      [TabStripItemIdentifier tabIdentifier:tab_switcher_item];

  EXPECT_EQ(item_identifier.itemType, TabStripItemTypeTab);
  EXPECT_NSEQ(item_identifier.tabSwitcherItem, tab_switcher_item);
}

// Verifies the equality of Tab strip tab items.
TEST_F(TabStripItemIdentifierTest, TabEqualTab) {
  web::WebStateID web_state_id = web::WebStateID::NewUnique();
  TabSwitcherItem* tab_switcher_item_1 =
      [[TabSwitcherItem alloc] initWithIdentifier:web_state_id];
  TabStripItemIdentifier* item_identifier_1 =
      [TabStripItemIdentifier tabIdentifier:tab_switcher_item_1];
  TabSwitcherItem* tab_switcher_item_2 =
      [[TabSwitcherItem alloc] initWithIdentifier:web_state_id];
  TabStripItemIdentifier* item_identifier_2 =
      [TabStripItemIdentifier tabIdentifier:tab_switcher_item_2];

  EXPECT_NSEQ(item_identifier_1, item_identifier_2);
  EXPECT_EQ(item_identifier_1.hash, item_identifier_2.hash);
}

// Verifies the inequality of Tab strip tab items.
TEST_F(TabStripItemIdentifierTest, TabNotEqualTab) {
  web::WebStateID web_state_id_1 = web::WebStateID::FromSerializedValue(42);
  TabSwitcherItem* tab_switcher_item_1 =
      [[TabSwitcherItem alloc] initWithIdentifier:web_state_id_1];
  TabStripItemIdentifier* item_identifier_1 =
      [TabStripItemIdentifier tabIdentifier:tab_switcher_item_1];
  web::WebStateID web_state_id_2 = web::WebStateID::FromSerializedValue(43);
  TabSwitcherItem* tab_switcher_item_2 =
      [[TabSwitcherItem alloc] initWithIdentifier:web_state_id_2];
  TabStripItemIdentifier* item_identifier_2 =
      [TabStripItemIdentifier tabIdentifier:tab_switcher_item_2];

  EXPECT_NSNE(item_identifier_1, item_identifier_2);
  EXPECT_NE(item_identifier_1.hash, item_identifier_2.hash);
  // Check also that hashes are not consecutive.
  EXPECT_NE(item_identifier_1.hash + 1, item_identifier_2.hash);
}

// Checks that TabStripItemIdentifier's hashing follows the current NSNumber
// implementation for tabs.
TEST_F(TabStripItemIdentifierTest, TabsHashingLikeNSNumber) {
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
    TabStripItemIdentifier* tab_item_identifier =
        [TabStripItemIdentifier tabIdentifier:tab_switcher_item];

    EXPECT_EQ(tab_item_identifier.hash, @(web_state_id.identifier()).hash);
  }
}

// Verifies the properties of a Tab strip group item.
TEST_F(TabStripItemIdentifierTest, Group) {
  FakeWebStateListDelegate web_state_list_delegate;
  std::unique_ptr<WebStateList> web_state_list =
      std::make_unique<WebStateList>(&web_state_list_delegate);
  web_state_list->InsertWebState(std::make_unique<web::FakeWebState>());
  const TabGroup* tab_group =
      web_state_list->CreateGroup({0}, {}, TabGroupId::GenerateNew());
  TabGroupItem* tab_group_item =
      [[TabGroupItem alloc] initWithTabGroup:tab_group
                                webStateList:web_state_list.get()];
  TabStripItemIdentifier* item_identifier =
      [TabStripItemIdentifier groupIdentifier:tab_group_item];

  EXPECT_EQ(item_identifier.itemType, TabStripItemTypeGroup);
  EXPECT_NSEQ(item_identifier.tabGroupItem, tab_group_item);
}

// Verifies the equality of Tab strip group items.
TEST_F(TabStripItemIdentifierTest, GroupEqualGroup) {
  FakeWebStateListDelegate web_state_list_delegate;
  std::unique_ptr<WebStateList> web_state_list =
      std::make_unique<WebStateList>(&web_state_list_delegate);
  web_state_list->InsertWebState(std::make_unique<web::FakeWebState>());
  const TabGroup* tab_group =
      web_state_list->CreateGroup({0}, {}, TabGroupId::GenerateNew());
  TabGroupItem* tab_group_item_1 =
      [[TabGroupItem alloc] initWithTabGroup:tab_group
                                webStateList:web_state_list.get()];
  TabStripItemIdentifier* item_identifier_1 =
      [TabStripItemIdentifier groupIdentifier:tab_group_item_1];
  TabGroupItem* tab_group_item_2 =
      [[TabGroupItem alloc] initWithTabGroup:tab_group
                                webStateList:web_state_list.get()];
  TabStripItemIdentifier* item_identifier_2 =
      [TabStripItemIdentifier groupIdentifier:tab_group_item_2];

  EXPECT_NSEQ(item_identifier_1, item_identifier_2);
  EXPECT_EQ(item_identifier_1.hash, item_identifier_2.hash);
}

// Verifies the inequality of Tab strip items.
TEST_F(TabStripItemIdentifierTest, GroupNotEqualGroup) {
  FakeWebStateListDelegate web_state_list_delegate;
  std::unique_ptr<WebStateList> web_state_list =
      std::make_unique<WebStateList>(&web_state_list_delegate);
  web_state_list->InsertWebState(std::make_unique<web::FakeWebState>());
  web_state_list->InsertWebState(std::make_unique<web::FakeWebState>());
  const TabGroup* tab_group_1 =
      web_state_list->CreateGroup({0}, {}, TabGroupId::GenerateNew());
  TabGroupItem* tab_group_item_1 =
      [[TabGroupItem alloc] initWithTabGroup:tab_group_1
                                webStateList:web_state_list.get()];
  TabStripItemIdentifier* item_identifier_1 =
      [TabStripItemIdentifier groupIdentifier:tab_group_item_1];
  const TabGroup* tab_group_2 =
      web_state_list->CreateGroup({1}, {}, TabGroupId::GenerateNew());
  TabGroupItem* tab_group_item_2 =
      [[TabGroupItem alloc] initWithTabGroup:tab_group_2
                                webStateList:web_state_list.get()];
  TabStripItemIdentifier* item_identifier_2 =
      [TabStripItemIdentifier groupIdentifier:tab_group_item_2];

  EXPECT_NSNE(item_identifier_1, item_identifier_2);
  EXPECT_NE(item_identifier_1.hash, item_identifier_2.hash);
  // Check also that hashes are not consecutive.
  EXPECT_NE(item_identifier_1.hash + 1, item_identifier_2.hash);
}

// Checks that TabStripItemIdentifier's hashing follows the current NSValue
// implementation.
TEST_F(TabStripItemIdentifierTest, GroupsHashingLikeNSValueWithPointer) {
  FakeWebStateListDelegate web_state_list_delegate;
  std::unique_ptr<WebStateList> web_state_list =
      std::make_unique<WebStateList>(&web_state_list_delegate);
  web_state_list->InsertWebState(std::make_unique<web::FakeWebState>());
  const TabGroup* tab_group =
      web_state_list->CreateGroup({0}, {}, TabGroupId::GenerateNew());
  TabGroupItem* tab_group_item =
      [[TabGroupItem alloc] initWithTabGroup:tab_group
                                webStateList:web_state_list.get()];
  TabStripItemIdentifier* item_identifier =
      [TabStripItemIdentifier groupIdentifier:tab_group_item];
  EXPECT_EQ(item_identifier.hash, GetHashForTabGroupItem(tab_group_item));
}

// Verifies the inequality of a Tab and a Group strip items.
TEST_F(TabStripItemIdentifierTest, TabNotEqualGroup) {
  web::WebStateID web_state_id = web::WebStateID::NewUnique();
  TabSwitcherItem* tab_switcher_item =
      [[TabSwitcherItem alloc] initWithIdentifier:web_state_id];
  TabStripItemIdentifier* tab_item_identifier =
      [TabStripItemIdentifier tabIdentifier:tab_switcher_item];
  FakeWebStateListDelegate web_state_list_delegate;
  std::unique_ptr<WebStateList> web_state_list =
      std::make_unique<WebStateList>(&web_state_list_delegate);
  web_state_list->InsertWebState(std::make_unique<web::FakeWebState>());
  const TabGroup* tab_group =
      web_state_list->CreateGroup({0}, {}, TabGroupId::GenerateNew());
  TabGroupItem* tab_group_item =
      [[TabGroupItem alloc] initWithTabGroup:tab_group
                                webStateList:web_state_list.get()];
  TabStripItemIdentifier* group_item_identifier =
      [TabStripItemIdentifier groupIdentifier:tab_group_item];

  EXPECT_NSNE(tab_item_identifier, group_item_identifier);
}

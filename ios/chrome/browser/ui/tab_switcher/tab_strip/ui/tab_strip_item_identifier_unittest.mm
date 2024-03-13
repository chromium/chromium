// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_strip/ui/swift.h"

#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_group_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_item.h"
#import "ios/web/public/web_state_id.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using TabStripItemIdentifierTest = PlatformTest;

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
  TabGroup tab_group{{}};
  TabGroupItem* tab_group_item =
      [[TabGroupItem alloc] initWithTabGroup:&tab_group];
  TabStripItemIdentifier* item_identifier =
      [TabStripItemIdentifier groupIdentifier:tab_group_item];

  EXPECT_EQ(item_identifier.itemType, TabStripItemTypeGroup);
  EXPECT_NSEQ(item_identifier.tabGroupItem, tab_group_item);
}

// Verifies the equality of Tab strip group items.
TEST_F(TabStripItemIdentifierTest, GroupEqualGroup) {
  TabGroup tab_group{{}};
  TabGroupItem* tab_group_item_1 =
      [[TabGroupItem alloc] initWithTabGroup:&tab_group];
  TabStripItemIdentifier* item_identifier_1 =
      [TabStripItemIdentifier groupIdentifier:tab_group_item_1];
  TabGroupItem* tab_group_item_2 =
      [[TabGroupItem alloc] initWithTabGroup:&tab_group];
  TabStripItemIdentifier* item_identifier_2 =
      [TabStripItemIdentifier groupIdentifier:tab_group_item_2];

  EXPECT_NSEQ(item_identifier_1, item_identifier_2);
  EXPECT_EQ(item_identifier_1.hash, item_identifier_2.hash);
}

// Verifies the inequality of Tab strip items.
TEST_F(TabStripItemIdentifierTest, GroupNotEqualGroup) {
  TabGroup tab_group_1{{}};
  TabGroupItem* tab_group_item_1 =
      [[TabGroupItem alloc] initWithTabGroup:&tab_group_1];
  TabStripItemIdentifier* item_identifier_1 =
      [TabStripItemIdentifier groupIdentifier:tab_group_item_1];
  TabGroup tab_group_2{{}};
  TabGroupItem* tab_group_item_2 =
      [[TabGroupItem alloc] initWithTabGroup:&tab_group_2];
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
  TabGroup tab_groups[] = {
      TabGroup{{}}, TabGroup{{}}, TabGroup{{}}, TabGroup{{}}, TabGroup{{}},
  };

  for (const auto& tab_group : tab_groups) {
    TabGroupItem* tab_group_item =
        [[TabGroupItem alloc] initWithTabGroup:&tab_group];
    TabStripItemIdentifier* item_identifier =
        [TabStripItemIdentifier groupIdentifier:tab_group_item];

    EXPECT_EQ(item_identifier.hash, GetHashForTabGroupItem(tab_group_item));
  }
}

// Verifies the inequality of a Tab and a Group strip items.
TEST_F(TabStripItemIdentifierTest, TabNotEqualGroup) {
  web::WebStateID web_state_id = web::WebStateID::NewUnique();
  TabSwitcherItem* tab_switcher_item =
      [[TabSwitcherItem alloc] initWithIdentifier:web_state_id];
  TabStripItemIdentifier* tab_item_identifier =
      [TabStripItemIdentifier tabIdentifier:tab_switcher_item];
  TabGroup tab_group{{}};
  TabGroupItem* tab_group_item =
      [[TabGroupItem alloc] initWithTabGroup:&tab_group];
  TabStripItemIdentifier* group_item_identifier =
      [TabStripItemIdentifier groupIdentifier:tab_group_item];

  EXPECT_NSNE(tab_item_identifier, group_item_identifier);
}

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_item_identifier.h"

#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_item.h"
#import "ios/web/public/web_state_id.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using GridItemIdentifierTest = PlatformTest;

// Verifies the properties of a Tab grid item.
TEST_F(GridItemIdentifierTest, Tab) {
  web::WebStateID web_state_id = web::WebStateID::NewUnique();
  TabSwitcherItem* tab_switcher_item =
      [[TabSwitcherItem alloc] initWithIdentifier:web_state_id];
  GridItemIdentifier* item_identifier =
      [GridItemIdentifier tabIdentifier:tab_switcher_item];

  EXPECT_EQ(item_identifier.type, GridItemType::Tab);
  EXPECT_NSEQ(item_identifier.tabSwitcherItem, tab_switcher_item);
}

// Verifies the properties of the SuggestedActions grid item.
TEST_F(GridItemIdentifierTest, SuggestedActions) {
  GridItemIdentifier* item_identifier =
      [GridItemIdentifier suggestedActionsIdentifier];

  EXPECT_EQ(item_identifier.type, GridItemType::SuggestedActions);
  EXPECT_NSEQ(item_identifier.tabSwitcherItem, nil);
}

// Verifies the equality of Tab grid items.
TEST_F(GridItemIdentifierTest, TabEqualTab) {
  web::WebStateID web_state_id = web::WebStateID::NewUnique();
  TabSwitcherItem* tab_switcher_item_1 =
      [[TabSwitcherItem alloc] initWithIdentifier:web_state_id];
  GridItemIdentifier* item_identifier_1 =
      [GridItemIdentifier tabIdentifier:tab_switcher_item_1];
  TabSwitcherItem* tab_switcher_item_2 =
      [[TabSwitcherItem alloc] initWithIdentifier:web_state_id];
  GridItemIdentifier* item_identifier_2 =
      [GridItemIdentifier tabIdentifier:tab_switcher_item_2];

  EXPECT_NSEQ(item_identifier_1, item_identifier_2);
  EXPECT_EQ(item_identifier_1.hash, item_identifier_2.hash);
}

// Verifies the inequality of Tab grid items.
TEST_F(GridItemIdentifierTest, TabNotEqualTab) {
  web::WebStateID web_state_id_1 = web::WebStateID::FromSerializedValue(42);
  TabSwitcherItem* tab_switcher_item_1 =
      [[TabSwitcherItem alloc] initWithIdentifier:web_state_id_1];
  GridItemIdentifier* item_identifier_1 =
      [GridItemIdentifier tabIdentifier:tab_switcher_item_1];
  web::WebStateID web_state_id_2 = web::WebStateID::FromSerializedValue(43);
  TabSwitcherItem* tab_switcher_item_2 =
      [[TabSwitcherItem alloc] initWithIdentifier:web_state_id_2];
  GridItemIdentifier* item_identifier_2 =
      [GridItemIdentifier tabIdentifier:tab_switcher_item_2];

  EXPECT_NSNE(item_identifier_1, item_identifier_2);
  EXPECT_NE(item_identifier_1.hash, item_identifier_2.hash);
  // Check also that hashes are not consecutive.
  EXPECT_NE(item_identifier_1.hash + 1, item_identifier_2.hash);
}

// Verifies the inequality of a Tab and a SuggestedTab grid items.
TEST_F(GridItemIdentifierTest, TabNotEqualSuggestedAction) {
  web::WebStateID web_state_id = web::WebStateID::NewUnique();
  TabSwitcherItem* tab_switcher_item =
      [[TabSwitcherItem alloc] initWithIdentifier:web_state_id];
  GridItemIdentifier* tab_item_identifier =
      [GridItemIdentifier tabIdentifier:tab_switcher_item];
  GridItemIdentifier* suggested_actions_item_identifier =
      [GridItemIdentifier suggestedActionsIdentifier];

  EXPECT_NSNE(tab_item_identifier, suggested_actions_item_identifier);
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
        [GridItemIdentifier tabIdentifier:tab_switcher_item];

    EXPECT_EQ(tab_item_identifier.hash, @(web_state_id.identifier()).hash);
  }
}

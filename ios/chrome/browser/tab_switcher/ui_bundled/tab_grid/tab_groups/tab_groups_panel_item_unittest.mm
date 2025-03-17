// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_panel_item.h"

#import <UIKit/UIKit.h>

#import "base/time/time.h"
#import "base/uuid.h"
#import "ios/chrome/browser/share_kit/model/sharing_state.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using tab_groups::SharingState;

using TabGroupsPanelItemTest = PlatformTest;

// Tests item type and payload are set at initialization.
TEST_F(TabGroupsPanelItemTest, Initialization) {
  TabGroupsPanelItem* notification_item =
      [[TabGroupsPanelItem alloc] initWithNotificationText:@"Text"];
  EXPECT_EQ(notification_item.type, TabGroupsPanelItemType::kNotification);
  EXPECT_NSEQ(notification_item.notificationText, @"Text");

  base::Uuid uuid = base::Uuid::GenerateRandomV4();
  TabGroupsPanelItem* saved_tab_group_item = [[TabGroupsPanelItem alloc]
      initWithSavedTabGroupID:uuid
                 sharingState:SharingState::kNotShared];
  EXPECT_EQ(saved_tab_group_item.type, TabGroupsPanelItemType::kSavedTabGroup);
  EXPECT_EQ(saved_tab_group_item.savedTabGroupID, uuid);
}

// Tests item's equality with the kNotification type.
TEST_F(TabGroupsPanelItemTest, NotificationEquality) {
  TabGroupsPanelItem* item_1 =
      [[TabGroupsPanelItem alloc] initWithNotificationText:@"One"];
  EXPECT_NSEQ(item_1, item_1);
  EXPECT_NSNE(item_1, nil);
  EXPECT_NSNE(item_1, [[NSObject alloc] init]);

  // A different text leads to inequality.
  TabGroupsPanelItem* item_2 =
      [[TabGroupsPanelItem alloc] initWithNotificationText:@"Two"];
  EXPECT_NSNE(item_1, item_2);

  // Reusing the first text leads to equality.
  TabGroupsPanelItem* item_3 =
      [[TabGroupsPanelItem alloc] initWithNotificationText:@"One"];
  EXPECT_NSEQ(item_1, item_3);
}

// Tests item's equality with the kSavedTabGroup type.
TEST_F(TabGroupsPanelItemTest, SavedTabGroupEquality) {
  base::Uuid first_uuid = base::Uuid::GenerateRandomV4();
  TabGroupsPanelItem* item_1 = [[TabGroupsPanelItem alloc]
      initWithSavedTabGroupID:first_uuid
                 sharingState:SharingState::kNotShared];
  EXPECT_NSEQ(item_1, item_1);
  EXPECT_NSNE(item_1, nil);
  EXPECT_NSNE(item_1, [[NSObject alloc] init]);

  // A different UUID leads to inequality.
  base::Uuid second_uuid = base::Uuid::GenerateRandomV4();
  TabGroupsPanelItem* item_2 = [[TabGroupsPanelItem alloc]
      initWithSavedTabGroupID:second_uuid
                 sharingState:SharingState::kNotShared];
  EXPECT_NSNE(item_1, item_2);

  // Reusing the first UUID leads to equality.
  TabGroupsPanelItem* item_3 = [[TabGroupsPanelItem alloc]
      initWithSavedTabGroupID:first_uuid
                 sharingState:SharingState::kNotShared];
  EXPECT_NSEQ(item_1, item_3);
}

// Tests item's inequality with non-matching types.
TEST_F(TabGroupsPanelItemTest, Inequality) {
  TabGroupsPanelItem* item_1 =
      [[TabGroupsPanelItem alloc] initWithNotificationText:@"Text"];
  base::Uuid uuid = base::Uuid::GenerateRandomV4();
  TabGroupsPanelItem* item_2 = [[TabGroupsPanelItem alloc]
      initWithSavedTabGroupID:uuid
                 sharingState:SharingState::kNotShared];
  EXPECT_NSNE(item_1, item_2);
}

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_groups_panel_item.h"

#import <UIKit/UIKit.h>

#import "base/time/time.h"
#import "base/uuid.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using TabGroupsPanelItemTest = PlatformTest;

// Tests item's equality.
TEST_F(TabGroupsPanelItemTest, Equality) {
  TabGroupsPanelItem* item_1 = [[TabGroupsPanelItem alloc] init];
  EXPECT_NSEQ(item_1, item_1);
  EXPECT_NSNE(item_1, nil);
  EXPECT_NSNE(item_1, [[NSObject alloc] init]);

  TabGroupsPanelItem* item_2 = [[TabGroupsPanelItem alloc] init];
  EXPECT_NSEQ(item_1, item_2);

  // Changing one saved tab group ID breaks equality.
  base::Uuid first_uuid = base::Uuid::GenerateRandomV4();
  item_1.savedTabGroupID = first_uuid;
  EXPECT_NSNE(item_1, item_2);

  // Setting the same saved tab group ID brings back equality.
  item_2.savedTabGroupID = first_uuid;
  EXPECT_NSEQ(item_1, item_2);
}

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_groups_panel_item.h"

#import <UIKit/UIKit.h>

#import "base/time/time.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using TabGroupsPanelItemTest = PlatformTest;

// Tests item's equality.
TEST_F(TabGroupsPanelItemTest, Equality) {
  // TODO(crbug.com/350493712): Compare based on SavedTabGroupID instead.
  TabGroupsPanelItem* item_1 = [[TabGroupsPanelItem alloc] init];
  EXPECT_NSEQ(item_1, item_1);
  EXPECT_NSNE(item_1, nil);
  EXPECT_NSNE(item_1, [[NSObject alloc] init]);

  TabGroupsPanelItem* item_2 = [[TabGroupsPanelItem alloc] init];
  EXPECT_NSEQ(item_1, item_2);

  // Changing one title breaks equality.
  item_1.title = @"Title";
  EXPECT_NSNE(item_1, item_2);

  // Setting the same title brings back equality.
  item_2.title = @"Title";
  EXPECT_NSEQ(item_1, item_2);

  // Changing the color doesn't affect equality.
  item_1.color = UIColor.yellowColor;
  item_2.color = UIColor.redColor;
  EXPECT_NSEQ(item_1, item_2);

  // Changing the creation date doesn't affect equality.
  item_1.creationDate = base::Time::Now();
  item_2.creationDate = base::Time::Now() - base::Days(1);
  EXPECT_NSEQ(item_1, item_2);

  // Changing the favicons doesn’t affect equality.
  item_1.favicons = @[ [[UIImage alloc] init] ];
  item_2.favicons = @[ [[UIImage alloc] init], [[UIImage alloc] init] ];
  EXPECT_NSEQ(item_1, item_2);
}

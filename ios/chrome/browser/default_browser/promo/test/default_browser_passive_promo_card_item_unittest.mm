// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_browser/promo/ui/default_browser_passive_promo_card_item.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/default_browser/promo/ui/default_browser_passive_promo_card_view.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_cell.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
using DefaultBrowserPassivePromoCardItemTest = PlatformTest;
}

// Tests that configuring the cell correctly passes properties from the item.
TEST_F(DefaultBrowserPassivePromoCardItemTest, ConfigureCellPassesProperties) {
  DefaultBrowserPassivePromoCardItem* item =
      [[DefaultBrowserPassivePromoCardItem alloc] initWithType:0];
  NSObject* testTarget = [[NSObject alloc] init];
  item.target = testTarget;
  item.closeAction = @selector(description);
  item.primaryAction = @selector(hash);

  EXPECT_EQ([DefaultBrowserPassivePromoCardCell class], [item cellClass]);

  LegacyTableViewCell* rawCell = [[[item cellClass] alloc] init];
  ASSERT_TRUE(
      [rawCell isMemberOfClass:[DefaultBrowserPassivePromoCardCell class]]);

  DefaultBrowserPassivePromoCardCell* cell =
      base::apple::ObjCCastStrict<DefaultBrowserPassivePromoCardCell>(rawCell);

  EXPECT_EQ(nil, cell.target);
  EXPECT_EQ(nil, cell.closeAction);
  EXPECT_EQ(nil, cell.primaryAction);

  [item configureCell:cell];

  EXPECT_EQ(testTarget, cell.target);
  EXPECT_EQ(@selector(description), cell.closeAction);
  EXPECT_EQ(@selector(hash), cell.primaryAction);

  [cell prepareForReuse];

  EXPECT_EQ(nil, cell.target);
  EXPECT_EQ(nil, cell.closeAction);
  EXPECT_EQ(nil, cell.primaryAction);
}

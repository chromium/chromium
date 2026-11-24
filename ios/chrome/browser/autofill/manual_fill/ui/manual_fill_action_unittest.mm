// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/manual_fill/ui/manual_fill_action.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/autofill/manual_fill/public/manual_fill_constants.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using ManualFillActionTest = PlatformTest;

// Tests that configuring a cell sets the blue title, background color,
// button trait, and accessibility identifier without an accessory view.
TEST_F(ManualFillActionTest, ConfiguresActionCell) {
  ManualFillAction* action = [[ManualFillAction alloc]
           initWithIdentifier:@"manage_passwords"
                        title:@"Manage passwords..."
      accessibilityIdentifier:manual_fill::
                                  kManagePasswordsAccessibilityIdentifier];

  UITableView* table_view = [[UITableView alloc] init];
  [TableViewCellContentConfiguration registerCellForTableView:table_view];
  UITableViewCell* cell =
      [TableViewCellContentConfiguration dequeueTableViewCell:table_view];
  ASSERT_NE(cell, nil);
  cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  [action configureCell:cell];

  EXPECT_NSEQ(action.identifier, @"manage_passwords");
  EXPECT_NSEQ(cell.backgroundColor,
              [UIColor colorNamed:kGroupedSecondaryBackgroundColor]);
  EXPECT_EQ(cell.accessoryType, UITableViewCellAccessoryNone);
  EXPECT_EQ(cell.accessibilityTraits, UIAccessibilityTraitButton);
  EXPECT_NSEQ(cell.accessibilityIdentifier,
              manual_fill::kManagePasswordsAccessibilityIdentifier);
  EXPECT_NSEQ(cell.accessibilityLabel, @"Manage passwords...");

  TableViewCellContentConfiguration* configuration =
      base::apple::ObjCCast<TableViewCellContentConfiguration>(
          cell.contentConfiguration);
  ASSERT_NE(configuration, nil);
  EXPECT_NSEQ(configuration.title, @"Manage passwords...");
  EXPECT_NSEQ(configuration.titleColor, [UIColor colorNamed:kBlueColor]);
  EXPECT_EQ(configuration.subtitle, nil);
  EXPECT_EQ(configuration.leadingConfiguration, nil);
}

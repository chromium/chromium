// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/settings_check_item.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/ui/settings/cells/settings_check_cell.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

using SettingsCheckItemTest = PlatformTest;

// Tests that the text and detail text are honoured after a call to
// `configureCell:`.
TEST_F(SettingsCheckItemTest, ConfigureCell) {
  SettingsCheckItem* item = [[SettingsCheckItem alloc] initWithType:0];
  NSString* text = @"Test Text";
  NSString* detailText = @"Test Detail Text that can span multiple lines. For "
                         @"example, this line probably fits on three or four "
                         @"lines.";

  item.text = text;
  item.detailText = detailText;

  id cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[SettingsCheckCell class]]);

  SettingsCheckCell* CheckCell =
      base::apple::ObjCCastStrict<SettingsCheckCell>(cell);
  EXPECT_FALSE(CheckCell.textLabel.text);
  EXPECT_FALSE(CheckCell.detailTextLabel.text);

  [item configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];
  EXPECT_NSEQ(text, CheckCell.textLabel.text);
  EXPECT_NSEQ(detailText, CheckCell.detailTextLabel.text);
}

// Tests that cell is configured properly based on infoButtonHidden property of
// the item.
TEST_F(SettingsCheckItemTest, InfoButtonVisibility) {
  SettingsCheckItem* item = [[SettingsCheckItem alloc] initWithType:0];
  item.text = @"Test Text";
  item.detailText = @"Test Text";
  item.enabled = YES;
  item.indicatorHidden = YES;
  item.infoButtonHidden = NO;

  id cell = [[[item cellClass] alloc] init];
  SettingsCheckCell* CheckCell =
      base::apple::ObjCCastStrict<SettingsCheckCell>(cell);

  [item configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];
  EXPECT_FALSE(CheckCell.infoButton.hidden);

  item.infoButtonHidden = YES;
  [item configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];
  EXPECT_TRUE(CheckCell.infoButton.hidden);
}

// Tests that infoButton won't be shown in case of a conflict.
TEST_F(SettingsCheckItemTest, InfoButtonVisibilityDuringConflict) {
  SettingsCheckItem* item = [[SettingsCheckItem alloc] initWithType:0];
  item.text = @"Test Text";
  item.detailText = @"Test Text";
  item.enabled = YES;
  item.indicatorHidden = NO;
  item.infoButtonHidden = NO;

  id cell = [[[item cellClass] alloc] init];
  SettingsCheckCell* CheckCell =
      base::apple::ObjCCastStrict<SettingsCheckCell>(cell);

  [item configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];
  EXPECT_TRUE(CheckCell.infoButton.hidden);
}

// Tests that infoButton would be greyed out when the item is not enabled.
TEST_F(SettingsCheckItemTest, InfoButtonVisibilityWhenDisabled) {
  SettingsCheckItem* item = [[SettingsCheckItem alloc] initWithType:0];
  item.text = @"Test Text";
  item.detailText = @"Test Text";
  item.enabled = NO;
  item.indicatorHidden = YES;
  item.infoButtonHidden = NO;

  id cell = [[[item cellClass] alloc] init];
  SettingsCheckCell* CheckCell =
      base::apple::ObjCCastStrict<SettingsCheckCell>(cell);

  [item configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];
  EXPECT_FALSE(CheckCell.infoButton.hidden);
  EXPECT_NSEQ(CheckCell.infoButton.tintColor,
              [UIColor colorNamed:kTextSecondaryColor]);
}

}  // namespace

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/cells/settings_check_item.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/legacy_table_view_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/activity_indicator_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/info_button_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
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
  ASSERT_TRUE([cell isMemberOfClass:[LegacyTableViewCell class]]);

  LegacyTableViewCell* checkCell =
      base::apple::ObjCCastStrict<LegacyTableViewCell>(cell);
  EXPECT_FALSE(checkCell.contentConfiguration);

  [item configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];

  TableViewCellContentConfiguration* contentConfiguration =
      base::apple::ObjCCastStrict<TableViewCellContentConfiguration>(
          checkCell.contentConfiguration);
  EXPECT_NSEQ(text, contentConfiguration.title);
  EXPECT_NSEQ(detailText, contentConfiguration.subtitle);
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
  // Target is required for the info button to be shown.
  item.infoButtonTarget = [[NSObject alloc] init];

  id cell = [[[item cellClass] alloc] init];
  LegacyTableViewCell* checkCell =
      base::apple::ObjCCastStrict<LegacyTableViewCell>(cell);

  [item configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];

  TableViewCellContentConfiguration* contentConfiguration =
      base::apple::ObjCCastStrict<TableViewCellContentConfiguration>(
          checkCell.contentConfiguration);
  EXPECT_TRUE([contentConfiguration.trailingConfiguration
      isKindOfClass:[InfoButtonContentConfiguration class]]);

  item.infoButtonHidden = YES;
  [item configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];

  contentConfiguration =
      base::apple::ObjCCastStrict<TableViewCellContentConfiguration>(
          checkCell.contentConfiguration);
  EXPECT_FALSE([contentConfiguration.trailingConfiguration
      isKindOfClass:[InfoButtonContentConfiguration class]]);
}

// Tests that infoButton won't be shown in case of a conflict.
TEST_F(SettingsCheckItemTest, InfoButtonVisibilityDuringConflict) {
  SettingsCheckItem* item = [[SettingsCheckItem alloc] initWithType:0];
  item.text = @"Test Text";
  item.detailText = @"Test Text";
  item.enabled = YES;
  item.indicatorHidden = NO;
  item.infoButtonHidden = NO;
  // Target is required for the info button to be shown.
  item.infoButtonTarget = [[NSObject alloc] init];

  id cell = [[[item cellClass] alloc] init];
  LegacyTableViewCell* checkCell =
      base::apple::ObjCCastStrict<LegacyTableViewCell>(cell);

  [item configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];

  TableViewCellContentConfiguration* contentConfiguration =
      base::apple::ObjCCastStrict<TableViewCellContentConfiguration>(
          checkCell.contentConfiguration);
  EXPECT_TRUE([contentConfiguration.trailingConfiguration
      isKindOfClass:[ActivityIndicatorContentConfiguration class]]);
}

// Tests that infoButton would be disabled when the item is not enabled.
TEST_F(SettingsCheckItemTest, InfoButtonVisibilityWhenDisabled) {
  SettingsCheckItem* item = [[SettingsCheckItem alloc] initWithType:0];
  item.text = @"Test Text";
  item.detailText = @"Test Text";
  item.enabled = NO;
  item.indicatorHidden = YES;
  item.infoButtonHidden = NO;
  // Target is required for the info button to be shown.
  item.infoButtonTarget = [[NSObject alloc] init];

  id cell = [[[item cellClass] alloc] init];
  LegacyTableViewCell* checkCell =
      base::apple::ObjCCastStrict<LegacyTableViewCell>(cell);

  [item configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];

  TableViewCellContentConfiguration* contentConfiguration =
      base::apple::ObjCCastStrict<TableViewCellContentConfiguration>(
          checkCell.contentConfiguration);
  EXPECT_TRUE([contentConfiguration.trailingConfiguration
      isKindOfClass:[InfoButtonContentConfiguration class]]);
  InfoButtonContentConfiguration* infoButtonConfiguration =
      base::apple::ObjCCastStrict<InfoButtonContentConfiguration>(
          contentConfiguration.trailingConfiguration);
  EXPECT_FALSE(infoButtonConfiguration.enabled);
}

}  // namespace

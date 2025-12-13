// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {
using TableViewTextItemTest = PlatformTest;
}

// Tests that the UILabels are set properly after a call to `configureCell:`.
TEST_F(TableViewTextItemTest, TextLabels) {
  NSString* text = @"Cell text";

  TableViewTextItem* item = [[TableViewTextItem alloc] initWithType:0];
  item.text = text;

  LegacyTableViewCell* cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[LegacyTableViewCell class]]);

  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];
  [item configureCell:cell withStyler:styler];

  ASSERT_TRUE([cell.contentConfiguration
      isMemberOfClass:TableViewCellContentConfiguration.class]);
  TableViewCellContentConfiguration* configuration =
      base::apple::ObjCCastStrict<TableViewCellContentConfiguration>(
          cell.contentConfiguration);

  EXPECT_NSEQ(text, configuration.title);
}

// Tests that item's text is shown as masked string in UILabel after a call to
// `configureCell:` with item.masked set to YES.
TEST_F(TableViewTextItemTest, MaskedTextLabels) {
  NSString* text = @"Cell text";

  TableViewTextItem* item = [[TableViewTextItem alloc] initWithType:0];
  item.text = text;
  item.masked = YES;

  LegacyTableViewCell* cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[LegacyTableViewCell class]]);

  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];
  [item configureCell:cell withStyler:styler];

  ASSERT_TRUE([cell.contentConfiguration
      isMemberOfClass:TableViewCellContentConfiguration.class]);
  TableViewCellContentConfiguration* configuration =
      base::apple::ObjCCastStrict<TableViewCellContentConfiguration>(
          cell.contentConfiguration);

  EXPECT_NSEQ(kMaskedPassword, configuration.title);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_HIDDEN_LABEL),
              cell.accessibilityLabel);
}

// Tests that item's text is attributed with a different font when using
// headlineFont.
TEST_F(TableViewTextItemTest, HeadlineFont) {
  NSString* text = @"Cell text";

  TableViewTextItem* item = [[TableViewTextItem alloc] initWithType:0];
  item.text = text;
  item.useHeadlineFont = YES;

  LegacyTableViewCell* cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[LegacyTableViewCell class]]);

  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];
  [item configureCell:cell withStyler:styler];

  ASSERT_TRUE([cell.contentConfiguration
      isMemberOfClass:TableViewCellContentConfiguration.class]);
  TableViewCellContentConfiguration* configuration =
      base::apple::ObjCCastStrict<TableViewCellContentConfiguration>(
          cell.contentConfiguration);

  EXPECT_NSEQ(text, configuration.attributedTitle.string);
  UIFont* font = [configuration.attributedTitle attribute:NSFontAttributeName
                                                  atIndex:0
                                           effectiveRange:nil];
  EXPECT_NSEQ([UIFont preferredFontForTextStyle:UIFontTextStyleHeadline], font);
}

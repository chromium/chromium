// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_details/cells/table_view_stacked_details_item.h"

#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

using TableViewStackedDetailsItemTest = PlatformTest;

TEST_F(TableViewStackedDetailsItemTest,
       CheckConfigureCellSetsCellAccessibilityLabel) {
  TableViewStackedDetailsItem* item =
      [[TableViewStackedDetailsItem alloc] initWithType:0];
  item.titleText = @"title";
  item.detailTexts =
      [NSArray<NSString*> arrayWithObjects:@"first", @"second", nil];

  id view = [[[item cellClass] alloc] init];
  ASSERT_TRUE([view isMemberOfClass:[TableViewStackedDetailsCell class]]);
  TableViewStackedDetailsCell* cell = view;

  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];
  [item configureCell:cell withStyler:styler];

  EXPECT_NSEQ(cell.accessibilityLabel, @"title, first, second");
  EXPECT_TRUE(cell.isAccessibilityElement);
}

TEST_F(TableViewStackedDetailsItemTest,
       CheckConfigureCellSetsTitleAndDetailLabels) {
  TableViewStackedDetailsItem* item =
      [[TableViewStackedDetailsItem alloc] initWithType:0];
  item.titleText = @"title";
  item.detailTexts =
      [NSArray<NSString*> arrayWithObjects:@"first", @"second", nil];

  id view = [[[item cellClass] alloc] init];
  ASSERT_TRUE([view isMemberOfClass:[TableViewStackedDetailsCell class]]);
  TableViewStackedDetailsCell* cell = view;

  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];
  [item configureCell:cell withStyler:styler];

  EXPECT_NSEQ(cell.titleLabel.text, @"title");

  ASSERT_EQ(cell.detailLabels.count, 2U);
  EXPECT_EQ(cell.detailLabels[0].superview.subviews.count, 2U);
  EXPECT_NSEQ(cell.detailLabels[0].text, @"first");
  EXPECT_NSEQ(cell.detailLabels[1].text, @"second");
}

TEST_F(TableViewStackedDetailsItemTest,
       CheckReconfigureCellSetsTitleAndDetailLabels) {
  TableViewStackedDetailsItem* item =
      [[TableViewStackedDetailsItem alloc] initWithType:0];
  item.titleText = @"title";
  item.detailTexts =
      [NSArray<NSString*> arrayWithObjects:@"first", @"second", nil];

  id view = [[[item cellClass] alloc] init];
  ASSERT_TRUE([view isMemberOfClass:[TableViewStackedDetailsCell class]]);
  TableViewStackedDetailsCell* cell = view;

  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];
  [item configureCell:cell withStyler:styler];

  EXPECT_NSEQ(cell.titleLabel.text, @"title");

  ASSERT_EQ(cell.detailLabels.count, 2U);
  EXPECT_EQ(cell.detailLabels[0].superview.subviews.count, 2U);
  EXPECT_NSEQ(cell.detailLabels[0].text, @"first");
  EXPECT_NSEQ(cell.detailLabels[1].text, @"second");

  // Reconfigure cell with more details.
  item.titleText = @"title1";
  item.detailTexts =
      [NSArray<NSString*> arrayWithObjects:@"first", @"second", @"third", nil];

  [item configureCell:cell withStyler:styler];

  EXPECT_NSEQ(cell.titleLabel.text, @"title1");

  ASSERT_EQ(cell.detailLabels.count, 3U);
  EXPECT_EQ(cell.detailLabels[0].superview.subviews.count, 3U);
  EXPECT_NSEQ(cell.detailLabels[0].text, @"first");
  EXPECT_NSEQ(cell.detailLabels[1].text, @"second");
  EXPECT_NSEQ(cell.detailLabels[2].text, @"third");

  // Reconfigure cell with less details.
  item.titleText = @"title";
  item.detailTexts =
      [NSArray<NSString*> arrayWithObjects:@"first", @"second", nil];

  [item configureCell:cell withStyler:styler];

  EXPECT_NSEQ(cell.titleLabel.text, @"title");

  ASSERT_EQ(cell.detailLabels.count, 2U);
  EXPECT_EQ(cell.detailLabels[0].superview.subviews.count, 2U);
  EXPECT_NSEQ(cell.detailLabels[0].text, @"first");
  EXPECT_NSEQ(cell.detailLabels[1].text, @"second");
}

TEST_F(TableViewStackedDetailsItemTest,
       CheckPrepareForReuseClearsTitleAndDetails) {
  TableViewStackedDetailsItem* item =
      [[TableViewStackedDetailsItem alloc] initWithType:0];
  item.titleText = @"title";
  item.detailTexts =
      [NSArray<NSString*> arrayWithObjects:@"first", @"second", nil];

  id view = [[[item cellClass] alloc] init];
  ASSERT_TRUE([view isMemberOfClass:[TableViewStackedDetailsCell class]]);
  TableViewStackedDetailsCell* cell = view;

  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];
  [item configureCell:cell withStyler:styler];

  EXPECT_NSEQ(cell.titleLabel.text, @"title");

  ASSERT_EQ(cell.detailLabels.count, 2U);

  UIView* details_container = cell.detailLabels[0].superview;

  [cell prepareForReuse];

  EXPECT_EQ(cell.detailLabels.count, 0U);
  EXPECT_EQ(details_container.subviews.count, 0U);
  EXPECT_NSEQ(cell.titleLabel.text, nil);
}

}  // namespace

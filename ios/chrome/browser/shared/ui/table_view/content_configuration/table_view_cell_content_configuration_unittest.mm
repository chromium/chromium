// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

using TableViewCellContentConfigurationTest = PlatformTest;

// Tests that accessibilityUserInputLabels returns an array with an empty string
// when the title is empty.
TEST_F(TableViewCellContentConfigurationTest,
       AccessibilityUserInputLabelsWithEmptyTitle) {
  TableViewCellContentConfiguration* config =
      [[TableViewCellContentConfiguration alloc] init];
  config.title = @"";

  NSArray<NSString*>* labels = [config accessibilityUserInputLabels];
  EXPECT_EQ(labels.count, 1u);
  EXPECT_NSEQ(labels[0], @"");
}

// Tests that accessibilityUserInputLabels returns what super returns when title
// is nil.
TEST_F(TableViewCellContentConfigurationTest,
       AccessibilityUserInputLabelsWithNilTitle) {
  TableViewCellContentConfiguration* config =
      [[TableViewCellContentConfiguration alloc] init];
  config.title = nil;

  NSArray<NSString*>* labels = [config accessibilityUserInputLabels];
  EXPECT_EQ(labels.count, 0u);
}

// Tests that verticalAlignment defaults to UIStackViewAlignmentCenter and
// can be copied.
TEST_F(TableViewCellContentConfigurationTest, TestVerticalAlignment) {
  TableViewCellContentConfiguration* config =
      [[TableViewCellContentConfiguration alloc] init];
  EXPECT_EQ(UIStackViewAlignmentCenter, config.verticalAlignment);

  config.verticalAlignment = UIStackViewAlignmentTop;
  TableViewCellContentConfiguration* copy = [config copy];
  EXPECT_EQ(UIStackViewAlignmentTop, copy.verticalAlignment);
}

// Tests that reservesLeadingSpace defaults to NO, can be copied, and adjusts
// separatorInsets.
TEST_F(TableViewCellContentConfigurationTest, TestReservesLeadingSpace) {
  TableViewCellContentConfiguration* config =
      [[TableViewCellContentConfiguration alloc] init];
  EXPECT_FALSE(config.reservesLeadingSpace);

  UIEdgeInsets defaultInsets = [config separatorInsets];

  config.reservesLeadingSpace = YES;
  TableViewCellContentConfiguration* copy = [config copy];
  EXPECT_TRUE(copy.reservesLeadingSpace);

  UIEdgeInsets reservedInsets = [config separatorInsets];
  EXPECT_GT(reservedInsets.left, defaultInsets.left);
}

// Tests that reservesLeadingSpace does not produce an ambiguous layout.
TEST_F(TableViewCellContentConfigurationTest,
       TestReservesLeadingSpaceAmbiguity) {
  TableViewCellContentConfiguration* config =
      [[TableViewCellContentConfiguration alloc] init];
  config.title = @"Test";
  config.reservesLeadingSpace = YES;
  UIView* view = [config makeContentView];
  view.frame = CGRectMake(0, 0, 320, 48);
  [view setNeedsLayout];
  [view layoutIfNeeded];
  EXPECT_FALSE([view hasAmbiguousLayout]);
  for (UIView* subview in view.subviews) {
    EXPECT_FALSE([subview hasAmbiguousLayout]);
    UIStackView* stack = base::apple::ObjCCast<UIStackView>(subview);
    if (stack) {
      UIView* leadingContainer = stack.arrangedSubviews[0];
      EXPECT_EQ(kTableViewIconImageSize, leadingContainer.frame.size.width);
      EXPECT_EQ(0.0, leadingContainer.frame.size.height);
    }
    for (UIView* subsubview in subview.subviews) {
      EXPECT_FALSE([subsubview hasAmbiguousLayout]);
    }
  }
}

}  // namespace

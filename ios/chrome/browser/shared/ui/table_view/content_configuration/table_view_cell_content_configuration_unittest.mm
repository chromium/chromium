// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"

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

}  // namespace

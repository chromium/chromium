// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_header_item.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using ContentSuggestionsHeaderItemTest = PlatformTest;

TEST_F(ContentSuggestionsHeaderItemTest, CellClass) {
  // Setup.
  ContentSuggestionsHeaderItem* item =
      [[ContentSuggestionsHeaderItem alloc] initWithType:0];

  // Action.
  ContentSuggestionsHeaderCell* cell = [[[item cellClass] alloc] init];

  // Test.
  EXPECT_EQ([ContentSuggestionsHeaderCell class], [cell class]);
}

TEST_F(ContentSuggestionsHeaderItemTest, Configure) {
  // Setup.
  UIView* view = [[UIView alloc] init];
  ContentSuggestionsHeaderItem* item =
      [[ContentSuggestionsHeaderItem alloc] initWithType:0];
  item.view = view;
  ContentSuggestionsHeaderCell* cell = [[[item cellClass] alloc] init];

  // Action.
  [item configureCell:cell];

  // Test.
  ASSERT_EQ(1U, [cell.contentView.subviews count]);
  EXPECT_EQ(view, cell.contentView.subviews[0]);
}
}

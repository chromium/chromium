// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_cell.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using ContentSuggestionsMostVisitedItemTest = PlatformTest;

TEST_F(ContentSuggestionsMostVisitedItemTest, CellClass) {
  // Setup.
  ContentSuggestionsMostVisitedItem* item =
      [[ContentSuggestionsMostVisitedItem alloc] initWithType:0];

  // Action.
  ContentSuggestionsMostVisitedCell* cell = [[[item cellClass] alloc] init];

  // Test.
  ASSERT_EQ([ContentSuggestionsMostVisitedCell class], [cell class]);
}

TEST_F(ContentSuggestionsMostVisitedItemTest, Configure) {
  // Setup.
  NSString* title = @"Test title.";
  ContentSuggestionsMostVisitedItem* item =
      [[ContentSuggestionsMostVisitedItem alloc] initWithType:0];
  item.title = title;
  item.attributes =
      [FaviconAttributes attributesWithMonogram:@"C"
                                      textColor:[UIColor whiteColor]
                                backgroundColor:[UIColor blackColor]
                         defaultBackgroundColor:NO];
  ContentSuggestionsMostVisitedCell* cell = [[[item cellClass] alloc] init];
  id faviconViewMock = OCMPartialMock(cell.faviconView);
  OCMExpect([faviconViewMock configureWithAttributes:item.attributes]);

  // Action.
  [item configureCell:cell];

  // Test.
  ASSERT_EQ(title, cell.titleLabel.text);
  ASSERT_OCMOCK_VERIFY(faviconViewMock);
}
}

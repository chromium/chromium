// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_whats_new_item.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using ContentSuggestionsWhatsNewItemTest = PlatformTest;

TEST_F(ContentSuggestionsWhatsNewItemTest, CellClass) {
  // Setup.
  ContentSuggestionsWhatsNewItem* item =
      [[ContentSuggestionsWhatsNewItem alloc] initWithType:0];

  // Action.
  ContentSuggestionsWhatsNewCell* cell = [[[item cellClass] alloc] init];

  // Test.
  EXPECT_EQ([ContentSuggestionsWhatsNewCell class], [cell class]);
}

TEST_F(ContentSuggestionsWhatsNewItemTest, Configure) {
  // Setup.
  UIImage* image = [[UIImage alloc] init];
  NSString* text = @"What's new test!";
  ContentSuggestionsWhatsNewItem* item =
      [[ContentSuggestionsWhatsNewItem alloc] initWithType:0];
  item.icon = image;
  item.text = text;
  id cell = OCMClassMock([ContentSuggestionsWhatsNewCell class]);
  OCMExpect([cell setIcon:image]);
  OCMExpect([cell setText:text]);

  // Action.
  [item configureCell:cell];

  // Test.
  ASSERT_OCMOCK_VERIFY(cell);
}
}

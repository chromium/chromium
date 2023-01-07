// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_category_wrapper.h"

#import "components/ntp_snippets/category.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Subclass used to test the equality.
@interface ContentSuggestionsCategoryWrapperSubclassTest
    : ContentSuggestionsCategoryWrapper
@end

@implementation ContentSuggestionsCategoryWrapperSubclassTest
@end

#pragma mark - Tests.

using ContentSuggestionsCategoryWrapperTest = PlatformTest;

// Tests that the category returned by the wrapper is the one given in the
// initializer.
TEST_F(ContentSuggestionsCategoryWrapperTest, GetCategory) {
  // Setup.
  ntp_snippets::Category category = ntp_snippets::Category::FromIDValue(2);
  ContentSuggestionsCategoryWrapper* wrapper =
      [[ContentSuggestionsCategoryWrapper alloc] initWithCategory:category];

  // Action/Tests.
  EXPECT_EQ(category, wrapper.category);
}

// Tests that two wrappers created with equal categories are equal.
TEST_F(ContentSuggestionsCategoryWrapperTest, AreWrappersEqual) {
  // Setup.
  ntp_snippets::Category category = ntp_snippets::Category::FromIDValue(2);
  ContentSuggestionsCategoryWrapper* wrapper =
      [[ContentSuggestionsCategoryWrapper alloc] initWithCategory:category];
  ntp_snippets::Category category2 = ntp_snippets::Category::FromIDValue(2);
  ContentSuggestionsCategoryWrapper* wrapper2 =
      [[ContentSuggestionsCategoryWrapper alloc] initWithCategory:category2];
  ASSERT_EQ(category, category2);

  // Action/Tests.
  EXPECT_TRUE([wrapper isEqual:wrapper2]);
  EXPECT_EQ(wrapper.hash, wrapper2.hash);
}

// Tests that two wrappers created with different categories are not equal.
TEST_F(ContentSuggestionsCategoryWrapperTest, AreWrappersDifferent) {
  // Setup.
  ntp_snippets::Category category = ntp_snippets::Category::FromIDValue(2);
  ContentSuggestionsCategoryWrapper* wrapper =
      [[ContentSuggestionsCategoryWrapper alloc] initWithCategory:category];
  ntp_snippets::Category category2 = ntp_snippets::Category::FromIDValue(3);
  ContentSuggestionsCategoryWrapper* wrapper2 =
      [[ContentSuggestionsCategoryWrapper alloc] initWithCategory:category2];
  ASSERT_NE(category, category2);

  // Action/Test.
  EXPECT_FALSE([wrapper isEqual:wrapper2]);
}

// Tests the equality between a wrapper an different type of objects.
TEST_F(ContentSuggestionsCategoryWrapperTest, DifferentObject) {
  // Setup.
  ntp_snippets::Category category = ntp_snippets::Category::FromIDValue(2);
  ContentSuggestionsCategoryWrapper* wrapper =
      [[ContentSuggestionsCategoryWrapper alloc] initWithCategory:category];
  NSObject* object = [[NSObject alloc] init];
  ContentSuggestionsCategoryWrapperSubclassTest* subclass =
      [[ContentSuggestionsCategoryWrapperSubclassTest alloc]
          initWithCategory:category];

  // Action/Tests.
  ASSERT_FALSE([wrapper isEqual:object]);
  ASSERT_TRUE([wrapper isEqual:subclass]);
}

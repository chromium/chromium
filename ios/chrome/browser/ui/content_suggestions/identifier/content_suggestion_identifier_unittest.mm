// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/identifier/content_suggestion_identifier.h"

#import "ios/chrome/browser/ui/content_suggestions/identifier/content_suggestions_section_information.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ContentSuggestionIdentifierSubclassTest : ContentSuggestionIdentifier
@end

@implementation ContentSuggestionIdentifierSubclassTest
@end

using ContentSuggestionIdentifierTest = PlatformTest;

// Tests different equality scenario.
TEST_F(ContentSuggestionIdentifierTest, IsEquals) {
  // Setup.
  std::string id1("identifier");
  std::string id2("identifier");
  ContentSuggestionsSectionInformation* sectionInfo =
      [[ContentSuggestionsSectionInformation alloc]
          initWithSectionID:ContentSuggestionsSectionMostVisited];

  ContentSuggestionIdentifier* suggestionIdentifier1 =
      [[ContentSuggestionIdentifier alloc] init];
  suggestionIdentifier1.sectionInfo = sectionInfo;
  suggestionIdentifier1.IDInSection = id1;

  ContentSuggestionIdentifier* suggestionIdentifier2 =
      [[ContentSuggestionIdentifier alloc] init];
  suggestionIdentifier2.sectionInfo = sectionInfo;
  suggestionIdentifier2.IDInSection = id2;

  ContentSuggestionIdentifierSubclassTest* subclass =
      [[ContentSuggestionIdentifierSubclassTest alloc] init];
  subclass.sectionInfo = sectionInfo;
  subclass.IDInSection = id2;

  // Action and Test.
  EXPECT_TRUE([suggestionIdentifier1 isEqual:suggestionIdentifier2]);
  EXPECT_TRUE([suggestionIdentifier1 isEqual:suggestionIdentifier1]);
  EXPECT_TRUE([suggestionIdentifier1 isEqual:subclass]);
}

// Test non-equality between different objects.
TEST_F(ContentSuggestionIdentifierTest, IsNotEqualsDifferentObjects) {
  // Setup.
  NSObject* object = [[NSObject alloc] init];
  ContentSuggestionIdentifier* suggestionIdentifier =
      [[ContentSuggestionIdentifier alloc] init];

  // Action and Test.
  EXPECT_FALSE([suggestionIdentifier isEqual:object]);
}

// Test non-equality.
TEST_F(ContentSuggestionIdentifierTest, IsNotEquals) {
  // Setup.
  std::string id1("identifier");
  std::string id2("identifier2");
  ContentSuggestionsSectionInformation* sectionInfo1 =
      [[ContentSuggestionsSectionInformation alloc]
          initWithSectionID:ContentSuggestionsSectionMostVisited];
  ContentSuggestionsSectionInformation* sectionInfo2 =
      [[ContentSuggestionsSectionInformation alloc]
          initWithSectionID:ContentSuggestionsSectionLogo];

  ContentSuggestionIdentifier* suggestionIdentifier1 =
      [[ContentSuggestionIdentifier alloc] init];
  suggestionIdentifier1.sectionInfo = sectionInfo1;
  suggestionIdentifier1.IDInSection = id1;

  ContentSuggestionIdentifier* suggestionIdentifier2 =
      [[ContentSuggestionIdentifier alloc] init];
  suggestionIdentifier2.sectionInfo = sectionInfo1;
  suggestionIdentifier2.IDInSection = id2;

  ContentSuggestionIdentifier* suggestionIdentifier3 =
      [[ContentSuggestionIdentifier alloc] init];
  suggestionIdentifier3.sectionInfo = sectionInfo2;
  suggestionIdentifier3.IDInSection = id1;

  // Action and Test.
  EXPECT_FALSE([suggestionIdentifier1 isEqual:suggestionIdentifier2]);
  EXPECT_FALSE([suggestionIdentifier2 isEqual:suggestionIdentifier3]);
  EXPECT_FALSE([suggestionIdentifier1 isEqual:suggestionIdentifier3]);
}

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_updater.h"

#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_text_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/suggested_content.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_data_source.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller.h"
#import "ios/chrome/browser/ui/content_suggestions/identifier/content_suggestion_identifier.h"
#import "ios/chrome/browser/ui/content_suggestions/identifier/content_suggestions_section_information.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using ContentSuggestionsCollectionUpdaterTest = PlatformTest;

TEST_F(ContentSuggestionsCollectionUpdaterTest, addEmptyItemToEmptySection) {
  // Setup.
  NSString* emptyString = @"test empty";
  id mockDataSource = OCMProtocolMock(@protocol(ContentSuggestionsDataSource));
  ContentSuggestionsCollectionUpdater* updater =
      [[ContentSuggestionsCollectionUpdater alloc] init];
  updater.dataSource = mockDataSource;
  CollectionViewModel* model = [[CollectionViewModel alloc] init];
  id mockCollection = OCMClassMock([ContentSuggestionsViewController class]);
  OCMStub([mockCollection collectionViewModel]).andReturn(model);
  updater.collectionViewController = mockCollection;

  CollectionViewItem<SuggestedContent>* suggestion =
      [[ContentSuggestionsTextItem alloc] initWithType:kItemTypeEnumZero];
  suggestion.suggestionIdentifier = [[ContentSuggestionIdentifier alloc] init];
  suggestion.suggestionIdentifier.sectionInfo =
      [[ContentSuggestionsSectionInformation alloc]
          initWithSectionID:ContentSuggestionsSectionMostVisited];
  suggestion.suggestionIdentifier.sectionInfo.showIfEmpty = YES;
  suggestion.suggestionIdentifier.sectionInfo.emptyText = emptyString;
  [updater addSectionsForSectionInfoToModel:@[
    suggestion.suggestionIdentifier.sectionInfo
  ]];
  ASSERT_EQ(0, [model numberOfItemsInSection:0]);

  // Action.
  NSIndexPath* addedItem = [updater addEmptyItemForSection:0];

  // Test.
  EXPECT_TRUE([[NSIndexPath indexPathForItem:0 inSection:0] isEqual:addedItem]);
  NSArray* items = [model
      itemsInSectionWithIdentifier:[model sectionIdentifierForSection:0]];
  ASSERT_EQ(1, [model numberOfItemsInSection:0]);
  ContentSuggestionsTextItem* item = items[0];
  EXPECT_EQ(emptyString, item.detailText);
}

TEST_F(ContentSuggestionsCollectionUpdaterTest,
       addEmptyItemToSectionWithoutText) {
  // Setup.
  id mockDataSource = OCMProtocolMock(@protocol(ContentSuggestionsDataSource));
  ContentSuggestionsCollectionUpdater* updater =
      [[ContentSuggestionsCollectionUpdater alloc] init];
  updater.dataSource = mockDataSource;
  CollectionViewModel* model = [[CollectionViewModel alloc] init];
  id mockCollection = OCMClassMock([ContentSuggestionsViewController class]);
  OCMStub([mockCollection collectionViewModel]).andReturn(model);
  updater.collectionViewController = mockCollection;

  CollectionViewItem<SuggestedContent>* suggestion =
      [[ContentSuggestionsTextItem alloc] initWithType:kItemTypeEnumZero];
  suggestion.suggestionIdentifier = [[ContentSuggestionIdentifier alloc] init];
  suggestion.suggestionIdentifier.sectionInfo =
      [[ContentSuggestionsSectionInformation alloc]
          initWithSectionID:ContentSuggestionsSectionMostVisited];
  suggestion.suggestionIdentifier.sectionInfo.showIfEmpty = YES;
  suggestion.suggestionIdentifier.sectionInfo.emptyText = nil;
  [updater addSectionsForSectionInfoToModel:@[
    suggestion.suggestionIdentifier.sectionInfo
  ]];
  ASSERT_EQ(0, [model numberOfItemsInSection:0]);

  // Action.
  NSIndexPath* addedItem = [updater addEmptyItemForSection:0];

  // Test.
  EXPECT_EQ(nil, addedItem);
  ASSERT_EQ(0, [model numberOfItemsInSection:0]);
}

TEST_F(ContentSuggestionsCollectionUpdaterTest, addEmptyItemToSection) {
  // Setup.
  id mockDataSource = OCMProtocolMock(@protocol(ContentSuggestionsDataSource));
  ContentSuggestionsCollectionUpdater* updater =
      [[ContentSuggestionsCollectionUpdater alloc] init];
  updater.dataSource = mockDataSource;
  CollectionViewModel* model = [[CollectionViewModel alloc] init];
  id mockCollection = OCMClassMock([ContentSuggestionsViewController class]);
  OCMStub([mockCollection collectionViewModel]).andReturn(model);
  updater.collectionViewController = mockCollection;

  CollectionViewItem<SuggestedContent>* suggestion =
      [[ContentSuggestionsTextItem alloc] initWithType:kItemTypeEnumZero];
  suggestion.suggestionIdentifier = [[ContentSuggestionIdentifier alloc] init];
  suggestion.suggestionIdentifier.sectionInfo =
      [[ContentSuggestionsSectionInformation alloc]
          initWithSectionID:ContentSuggestionsSectionMostVisited];
  suggestion.suggestionIdentifier.sectionInfo.showIfEmpty = YES;
  suggestion.suggestionIdentifier.sectionInfo.emptyText = @"empty";
  [updater addSectionsForSectionInfoToModel:@[
    suggestion.suggestionIdentifier.sectionInfo
  ]];
  [updater addSuggestionsToModel:@[ suggestion ]
                 withSectionInfo:suggestion.suggestionIdentifier.sectionInfo];
  ASSERT_EQ(1, [model numberOfItemsInSection:0]);

  // Action.
  NSIndexPath* addedItem = [updater addEmptyItemForSection:0];

  // Test.
  EXPECT_TRUE([[NSIndexPath indexPathForItem:1 inSection:0] isEqual:addedItem]);
  ASSERT_EQ(2, [model numberOfItemsInSection:0]);
}

}  // namespace

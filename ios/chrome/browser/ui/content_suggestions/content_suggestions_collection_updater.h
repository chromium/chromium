// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_COLLECTION_UPDATER_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_COLLECTION_UPDATER_H_

#import <UIKit/UIKit.h>

@class CollectionViewItem;
@class ContentSuggestionsSectionInformation;
@class ContentSuggestionsViewController;
@protocol ContentSuggestionsDataSource;
@protocol SuggestedContent;

// Enum defining the type of a ContentSuggestions.
typedef NS_ENUM(NSInteger, ContentSuggestionType) {
  // Use this type to pass information about an empty section. Suggestion of
  // this type are empty and should not be displayed. The informations to be
  // displayed are contained in the SectionInfo.
  ContentSuggestionTypeEmpty,
  ContentSuggestionTypeMostVisited,
  ContentSuggestionTypeReturnToRecentTab,
  ContentSuggestionTypePromo,
};

// Updater for a CollectionViewController populating it with some items and
// handling the items addition.
@interface ContentSuggestionsCollectionUpdater : NSObject

// Data source for this object.
@property(nonatomic, weak) id<ContentSuggestionsDataSource> dataSource;

// |collectionViewController| this Updater will update. Needs to be set before
// adding items.
@property(nonatomic, weak)
    ContentSuggestionsViewController* collectionViewController;

// Returns whether the section should use the default, non-card style.
- (BOOL)shouldUseCustomStyleForSection:(NSInteger)section;

// Returns the ContentSuggestionType associated with this item.
- (ContentSuggestionType)contentSuggestionTypeForItem:(CollectionViewItem*)item;

// Adds the sections for the |suggestions| to the model and returns their
// indices.
- (NSIndexSet*)addSectionsForSectionInfoToModel:
    (NSArray<ContentSuggestionsSectionInformation*>*)sectionsInfo;

// Adds the |suggestions| to the model in the section corresponding to
// |sectionInfo| and returns their index paths. The caller must ensure the
// corresponding section has been added to the model.
- (NSArray<NSIndexPath*>*)
addSuggestionsToModel:
    (NSArray<CollectionViewItem<SuggestedContent>*>*)suggestions
      withSectionInfo:(ContentSuggestionsSectionInformation*)sectionInfo;

// Adds an empty item to this |section| and returns its index path. The updater
// does not do any check about the number of elements in the section.
// Returns nil if there is no empty item for this section.
- (NSIndexPath*)addEmptyItemForSection:(NSInteger)section;

// Returns whether |section| contains the Return to Recent Tab tile.
- (BOOL)isReturnToRecentTabSection:(NSInteger)section;

// Returns whether |section| contains the Most Visited tiles.
- (BOOL)isMostVisitedSection:(NSInteger)section;

// Returns whether |section| contains the What's New promo.
- (BOOL)isPromoSection:(NSInteger)section;

// Returns whether |section| contains the promo if there is one and with a
// header containing the fake omnibox and the logo.
- (BOOL)isHeaderSection:(NSInteger)section;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_COLLECTION_UPDATER_H_

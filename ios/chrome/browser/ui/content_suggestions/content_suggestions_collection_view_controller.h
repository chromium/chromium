// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_COLLECTION_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_COLLECTION_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/collection_view/collection_view_controller.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/suggested_content.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_consumer.h"

namespace {

using CSCollectionViewModel = CollectionViewModel<CSCollectionViewItem*>;

// Enum defining the type of a ContentSuggestions.
typedef NS_ENUM(NSInteger, ContentSuggestionType) {
  // Use this type to pass information about an empty section. Suggestion of
  // this type are empty and should not be displayed. The information to be
  // displayed are contained in the SectionInfo.
  ContentSuggestionTypeEmpty,
  ContentSuggestionTypeMostVisited,
  ContentSuggestionTypeReturnToRecentTab,
  ContentSuggestionTypePromo,
};

// Enum defining the ItemTypes of this ContentSuggestionsViewController.
typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeFooter = kItemTypeEnumZero,
  ItemTypeHeader,
  ItemTypeEmpty,
  ItemTypeMostVisited,
  ItemTypePromo,
  ItemTypeReturnToRecentTab,
  ItemTypeSingleCell,
  ItemTypeUnknown,
};

// Enum defining the SectionIdentifiers of this
// ContentSuggestionsViewController.
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierMostVisited = kSectionIdentifierEnumZero,
  SectionIdentifierLogo,
  SectionIdentifierReturnToRecentTab,
  SectionIdentifierPromo,
  SectionIdentifierSingleCell,
  SectionIdentifierDefault,
};

}  // namespace

@class ContentSuggestionsSectionInformation;
@protocol ContentSuggestionsActionHandler;
@protocol ContentSuggestionsCommands;
@protocol ContentSuggestionsHeaderControlling;
@protocol ContentSuggestionsMenuProvider;
@protocol ContentSuggestionsViewControllerAudience;
@protocol SuggestedContent;

// CollectionViewController to display the suggestions items.
@interface ContentSuggestionsCollectionViewController
    : CollectionViewController <ContentSuggestionsCollectionConsumer>

// Inits view controller with |style|.
- (instancetype)initWithStyle:(CollectionViewControllerStyle)style
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithLayout:(UICollectionViewLayout*)layout
                         style:(CollectionViewControllerStyle)style
    NS_UNAVAILABLE;

// Handler for the commands sent by the ContentSuggestionsViewController.
@property(nonatomic, weak) id<ContentSuggestionsCommands>
    suggestionCommandHandler;
@property(nonatomic, weak) id<ContentSuggestionsViewControllerAudience>
    audience;
// Override from superclass to have a more specific type.
@property(nonatomic, readonly)
    CollectionViewModel<CollectionViewItem<SuggestedContent>*>*
        collectionViewModel;
// Whether or not the contents section should be hidden completely.
@property(nonatomic, assign) BOOL contentSuggestionsEnabled;
// Provides information about the content suggestions header. Used to get the
// header height.
// TODO(crbug.com/1114792): Remove this and replace its call with refactored
// header synchronizer.
@property(nonatomic, weak) id<ContentSuggestionsHeaderControlling>
    headerProvider;
// Delegate for handling actions relating to content suggestions.
@property(nonatomic, weak) id<ContentSuggestionsActionHandler> handler;
// Provider of menu configurations for the contentSuggestions component.
@property(nonatomic, weak) id<ContentSuggestionsMenuProvider> menuProvider;
// Returns the header view containing the logo and omnibox to be displayed.
- (UIView*)headerViewForWidth:(CGFloat)width;
@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_COLLECTION_VIEW_CONTROLLER_H_

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/collection_view/collection_view_controller.h"

@class BubblePresenter;
@class ContentSuggestionsSectionInformation;
@protocol ContentSuggestionsActionHandler;
@protocol ContentSuggestionsCommands;
@protocol ContentSuggestionsDataSource;
@protocol ContentSuggestionsHeaderControlling;
@protocol ContentSuggestionsMenuProvider;
@protocol ContentSuggestionsViewControllerAudience;
@protocol DiscoverFeedHeaderChanging;
@protocol DiscoverFeedMenuCommands;
@protocol SnackbarCommands;
@protocol SuggestedContent;
@protocol ThemeChangeDelegate;

// CollectionViewController to display the suggestions items.
@interface ContentSuggestionsViewController : CollectionViewController

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
// Delegate for handling theme changes (dark/light theme).
@property(nonatomic, weak) id<ThemeChangeDelegate> themeChangeDelegate;
@property(nonatomic, weak) id<DiscoverFeedMenuCommands> discoverFeedMenuHandler;
@property(nonatomic, weak, readonly) id<DiscoverFeedHeaderChanging>
    discoverFeedHeaderDelegate;
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

// Bubble presenter for displaying IPH bubbles relating to the NTP.
@property(nonatomic, strong) BubblePresenter* bubblePresenter;

- (void)setDataSource:(id<ContentSuggestionsDataSource>)dataSource;
- (void)setDispatcher:(id<SnackbarCommands>)dispatcher;

// Removes the entry at |indexPath|, from the collection and its model.
- (void)dismissEntryAtIndexPath:(NSIndexPath*)indexPath;
// Removes the |section|.
- (void)dismissSection:(NSInteger)section;
// Adds the |suggestions| to the collection and its model in the section
// corresponding to |sectionInfo|.
- (void)addSuggestions:
            (NSArray<CollectionViewItem<SuggestedContent>*>*)suggestions
         toSectionInfo:(ContentSuggestionsSectionInformation*)sectionInfo;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_VIEW_CONTROLLER_H_

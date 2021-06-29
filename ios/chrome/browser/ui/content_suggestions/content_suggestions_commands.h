// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_COMMANDS_H_

@class CollectionViewItem;

// Commands protocol allowing the ContentSuggestions ViewControllers to interact
// with the coordinator layer, and from there to the rest of the application.
@protocol ContentSuggestionsCommands

// Opens the Reading List.
- (void)openReadingList;
// Opens the page associated with the item at |indexPath|.
- (void)openPageForItemAtIndexPath:(NSIndexPath*)indexPath;
// Opens the Most Visited associated with this |item| at the |mostVisitedItem|.
- (void)openMostVisitedItem:(CollectionViewItem*)item
                    atIndex:(NSInteger)mostVisitedIndex;
// Handles the actions tapping the "Return to Recent Tab" item that returns the
// user to the last opened tab.
- (void)openMostRecentTab:(CollectionViewItem*)item;
// Handles the action that dismisses the "Return to Recent Tab" item.
- (void)hideMostRecentTab;
// Displays a context menu for the |suggestionItem|.
- (void)displayContextMenuForSuggestion:(CollectionViewItem*)suggestionItem
                                atPoint:(CGPoint)touchLocation
                            atIndexPath:(NSIndexPath*)indexPath
                        readLaterAction:(BOOL)readLaterAction;
// Displays a context menu for the |mostVisitedItem|.
- (void)displayContextMenuForMostVisitedItem:
            (CollectionViewItem*)mostVisitedItem
                                     atPoint:(CGPoint)touchLocation
                                 atIndexPath:(NSIndexPath*)indexPath;
// Dismisses the context menu if it is displayed.
- (void)dismissModals;
// Handles the actions following a tap on the promo.
- (void)handlePromoTapped;
// Handles the actions following a tap on the "Learn more" item.
- (void)handleLearnMoreTapped;
// Handles the actions following a tap on the "Manage Activity" item in the
// Discover feed menu.
- (void)handleFeedManageActivityTapped;
// Handles the actions following a tap on the "Manage Interests" item in the
// Discover feed menu.
- (void)handleFeedManageInterestsTapped;
// Handles the actions following a tap on the "Learn More" item in the Discover
// feed menu.
- (void)handleFeedLearnMoreTapped;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_COMMANDS_H_

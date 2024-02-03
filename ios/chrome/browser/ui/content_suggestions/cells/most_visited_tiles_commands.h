// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_MOST_VISITED_TILES_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_MOST_VISITED_TILES_COMMANDS_H_

#import <UIKit/UIKit.h>

@class ContentSuggestionsMostVisitedItem;

// Command protocol for events for the Most Visited Tiles.
@protocol MostVisitedTilesCommands

// Indicates to the receiver that a Most Visited tile `sender` was tapped.
- (void)mostVisitedTileTapped:(UIGestureRecognizer*)sender;

// Open the URL corresponding to the `item` in a new tab, `incognito` or not.
// Animate the opening of a new tab from `point`.
// The item has to be a Most Visited item.
- (void)openNewTabWithMostVisitedItem:(ContentSuggestionsMostVisitedItem*)item
                            incognito:(BOOL)incognito
                              atIndex:(NSInteger)mostVisitedIndex
                            fromPoint:(CGPoint)point;

// Open the URL corresponding to the `item` in a new tab, `incognito` or not.
// The item has to be a Most Visited item.
- (void)openNewTabWithMostVisitedItem:(ContentSuggestionsMostVisitedItem*)item
                            incognito:(BOOL)incognito
                              atIndex:(NSInteger)mostVisitedIndex;

// Open the URL corresponding to the `item` in a new tab, `incognito` or not.
// The index of the item will be find by the  command handler. The item has to
// be a Most Visited item.
- (void)openNewTabWithMostVisitedItem:(ContentSuggestionsMostVisitedItem*)item
                            incognito:(BOOL)incognito;

// Removes the most visited `item`.
- (void)removeMostVisited:(ContentSuggestionsMostVisitedItem*)item;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_MOST_VISITED_TILES_COMMANDS_H_

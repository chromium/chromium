// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_MOST_VISITED_TILES_UI_MOST_VISITED_TILES_COMMANDS_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_MOST_VISITED_TILES_UI_MOST_VISITED_TILES_COMMANDS_H_

#import <UIKit/UIKit.h>

@class MostVisitedItem;

// Command protocol for events for the Most Visited Tiles.
@protocol MostVisitedTilesCommands

// Indicates to the receiver that a Most Visited tile `sender` was tapped.
- (void)mostVisitedTileTapped:(UIGestureRecognizer*)sender;

// Open the URL corresponding to the `item` in a new tab, `incognito` or not.
// Animate the opening of a new tab from `point`.
// The item has to be a Most Visited item.
- (void)openNewTabWithMostVisitedItem:(MostVisitedItem*)item
                            incognito:(BOOL)incognito
                              atIndex:(NSInteger)mostVisitedIndex
                            fromPoint:(CGPoint)point;

// Open the URL corresponding to the `item` in a new tab, `incognito` or not.
// The item has to be a Most Visited item.
- (void)openNewTabWithMostVisitedItem:(MostVisitedItem*)item
                            incognito:(BOOL)incognito
                              atIndex:(NSInteger)mostVisitedIndex;

// Open the URL corresponding to the `item` in a new tab, `incognito` or not.
// The index of the item will be find by the  command handler. The item has to
// be a Most Visited item.
- (void)openNewTabWithMostVisitedItem:(MostVisitedItem*)item
                            incognito:(BOOL)incognito;

// Pins or unpins the item to/from the most visited tile, depending on whether
// the item is already pinned or not.
- (void)pinOrUnpinMostVisited:(MostVisitedItem*)item;

// Removes the most visited `item`.
- (void)removeMostVisited:(MostVisitedItem*)item;

// Reorders the most visited `item` to `toIndex`.
- (void)moveMostVisitedItem:(MostVisitedItem*)item toIndex:(NSUInteger)index;

// Opens the modal for user to add a new pinned site to the most visited tiles.
- (void)openModalToAddPinnedSite;

// Opens the modal for user to edit an existing pinned site on the most visited
// tiles.
- (void)openModalToEditPinnedSite:(MostVisitedItem*)item;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_MOST_VISITED_TILES_UI_MOST_VISITED_TILES_COMMANDS_H_

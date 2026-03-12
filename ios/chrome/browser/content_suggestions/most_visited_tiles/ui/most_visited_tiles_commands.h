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

// Reorders the most visited `item` to `toIndex`.
- (void)moveMostVisitedItem:(MostVisitedItem*)item toIndex:(NSUInteger)index;

// Opens the modal for user to add a new pinned site to the most visited tiles.
- (void)openModalToAddPinnedSite;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_MOST_VISITED_TILES_UI_MOST_VISITED_TILES_COMMANDS_H_

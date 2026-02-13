// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_MOST_VISITED_TILES_UI_MOST_VISITED_TILES_CONFIG_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_MOST_VISITED_TILES_UI_MOST_VISITED_TILES_CONFIG_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/content_suggestions/magic_stack/ui/magic_stack_module.h"

@protocol ContentSuggestionsImageDataSource;
@class LayoutGuideCenter;
@protocol MostVisitedTilesCommands;
@class MostVisitedItem;

// Config object for the Most Visited Tiles module.
@interface MostVisitedTilesConfig : MagicStackModule

// The updates to properties must be reflected in the copy method.
// LINT.IfChange(Copy)
// List of Most Visited Tiles to show in module.
@property(nonatomic, strong) NSArray<MostVisitedItem*>* mostVisitedItems;

// The layout guide center to reference the first cell of the most visited
// tiles.
@property(nonatomic, readonly) LayoutGuideCenter* layoutGuideCenter;

// Data source for the most visited tiles favicon.
@property(nonatomic, weak) id<ContentSuggestionsImageDataSource>
    imageDataSource;

// Command handler for user actions.
@property(nonatomic, weak) id<MostVisitedTilesCommands> commandHandler;
// LINT.ThenChange(most_visited_tiles_config.mm:Copy)

// Initializes the config object for most visited tiles.
- (instancetype)initWithLayoutGuideCenter:(LayoutGuideCenter*)layoutGuideCenter
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_MOST_VISITED_TILES_UI_MOST_VISITED_TILES_CONFIG_H_

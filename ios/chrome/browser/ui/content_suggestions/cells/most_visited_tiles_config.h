// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_MOST_VISITED_TILES_CONFIG_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_MOST_VISITED_TILES_CONFIG_H_

#import <UIKit/UIKit.h>

@protocol MostVisitedTilesCommands;
@protocol MostVisitedTilesStackViewConsumerSource;
@class ContentSuggestionsMostVisitedItem;
@protocol ContentSuggestionsImageDataSource;

#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_module.h"

// Config object for the Most Visited Tiles module.
@interface MostVisitedTilesConfig : MagicStackModule

// List of Most Visited Tiles to show in module.
@property(nonatomic, strong)
    NSArray<ContentSuggestionsMostVisitedItem*>* mostVisitedItems;

// Data source for the most visited tiles favicon.
@property(nonatomic, weak) id<ContentSuggestionsImageDataSource>
    imageDataSource;

// Most Visited Tiles model.
@property(nonatomic, weak)
    id<MostVisitedTilesStackViewConsumerSource> consumerSource;

// Command handler for user actions.
@property(nonatomic, weak) id<MostVisitedTilesCommands> commandHandler;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_MOST_VISITED_TILES_CONFIG_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_MOST_VISITED_TILES_UI_MOST_VISITED_TILES_CONFIG_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_MOST_VISITED_TILES_UI_MOST_VISITED_TILES_CONFIG_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/content_suggestions/magic_stack/ui/magic_stack_module.h"

@protocol ContentSuggestionsImageDataSource;
@protocol MostVisitedTilesCommands;
@class MostVisitedItem;

// Config object for the Most Visited Tiles module.
@interface MostVisitedTilesConfig : MagicStackModule

// List of Most Visited Tiles to show in module.
@property(nonatomic, strong) NSArray<MostVisitedItem*>* mostVisitedItems;

// Data source for the most visited tiles favicon.
@property(nonatomic, weak) id<ContentSuggestionsImageDataSource>
    imageDataSource;

// Command handler for user actions.
@property(nonatomic, weak) id<MostVisitedTilesCommands> commandHandler;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_MOST_VISITED_TILES_UI_MOST_VISITED_TILES_CONFIG_H_

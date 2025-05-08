// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_CELLS_MOST_VISITED_TILES_STACK_VIEW_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_CELLS_MOST_VISITED_TILES_STACK_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/content_suggestions/ui_bundled/cells/most_visited_tiles_stack_view_consumer.h"

@protocol MagicStackModuleContentViewDelegate;
@class MostVisitedTilesConfig;

// Implementation for the Most Visited Tiles so that its model can directly
// update it.
@interface MostVisitedTilesStackView
    : UIStackView <MostVisitedTilesStackViewConsumer>

// Initializes it with `config`, `contentViewDelegate` and `spacing` between
// each tile.
// TODO(crbug.com/391617946): Refactor content view delegate and methods that
// use it out of the initializer.
- (instancetype)initWithConfig:(MostVisitedTilesConfig*)config
                       spacing:(CGFloat)spacing;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_CELLS_MOST_VISITED_TILES_STACK_VIEW_H_

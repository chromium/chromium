// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_MOST_VISITED_TILES_STACK_VIEW_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_MOST_VISITED_TILES_STACK_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/content_suggestions/cells/most_visited_tiles_stack_view_consumer.h"

@class MostVisitedTilesConfig;

// Implementation for the Most Visited Tiles so that its model can directly
// update it.
@interface MostVisitedTilesStackView
    : UIStackView <MostVisitedTilesStackViewConsumer>

// Initializes it with `config` and `spacing` between each tile.
- (instancetype)initWithConfig:(MostVisitedTilesConfig*)config
                       spacing:(CGFloat)spacing;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_MOST_VISITED_TILES_STACK_VIEW_H_

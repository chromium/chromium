// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_MOST_VISITED_TILES_STACK_VIEW_CONSUMER_SOURCE_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_MOST_VISITED_TILES_STACK_VIEW_CONSUMER_SOURCE_H_

@protocol MostVisitedTilesStackViewConsumer;

// The source of any consumer of Most Visited Tiles MagicStack events.
@protocol MostVisitedTilesStackViewConsumerSource

// Consumer for this model.
- (void)addConsumer:(id<MostVisitedTilesStackViewConsumer>)consumer;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_MOST_VISITED_TILES_STACK_VIEW_CONSUMER_SOURCE_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_MOST_VISITED_TILES_STACK_VIEW_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_MOST_VISITED_TILES_STACK_VIEW_CONSUMER_H_

#include <CoreFoundation/CoreFoundation.h>

@class MostVisitedTilesConfig;

// Interface for listening to events occurring in MostVisitedTilesMediator.
@protocol MostVisitedTilesStackViewConsumer

// Indicates to the consumer that it should update for the latest changes to
// `config`.
- (void)updateWithConfig:(MostVisitedTilesConfig*)config;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_MOST_VISITED_TILES_STACK_VIEW_CONSUMER_H_

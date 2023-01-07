// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_FAVICON_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_FAVICON_MEDIATOR_H_

#import <UIKit/UIKit.h>

#include "components/ntp_tiles/ntp_tile.h"

namespace favicon {
class LargeIconService;
}

@protocol ContentSuggestionsCollectionConsumer;
@protocol ContentSuggestionsConsumer;
@class ContentSuggestionsMostVisitedItem;
@class FaviconAttributesProvider;
class LargeIconCache;

// Mediator handling the fetching of the favicon for all ContentSuggestions
// items.
@interface ContentSuggestionsFaviconMediator : NSObject

// Initializes the mediator with `largeIconService` to fetch the favicon
// locally.
- (instancetype)initWithLargeIconService:
                    (favicon::LargeIconService*)largeIconService
                          largeIconCache:(LargeIconCache*)largeIconCache
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@property(nonatomic, weak) id<ContentSuggestionsConsumer> consumer;

// FaviconAttributesProvider to fetch the favicon for the most visited tiles.
@property(nonatomic, strong, readonly)
    FaviconAttributesProvider* mostVisitedAttributesProvider;

// Sets the `mostVisitedData` used to log the impression of the tiles.
- (void)setMostVisitedDataForLogging:
    (const ntp_tiles::NTPTilesVector&)mostVisitedData;

// Fetches the favicon for this `item`.
- (void)fetchFaviconForMostVisited:(ContentSuggestionsMostVisitedItem*)item;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_FAVICON_MEDIATOR_H_

// Copyright 2017 The Chromium Authors. All rights reserved.
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
@class ContentSuggestionsParentItem;
@class FaviconAttributesProvider;
class LargeIconCache;

// Mediator handling the fetching of the favicon for all ContentSuggestions
// items.
@interface ContentSuggestionsFaviconMediator : NSObject

// Initializes the mediator with |largeIconService| to fetch the favicon
// locally.
- (instancetype)initWithLargeIconService:
                    (favicon::LargeIconService*)largeIconService
                          largeIconCache:(LargeIconCache*)largeIconCache
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// The consumer that will be notified when the data change. |consumer| is used
// if kContentSuggestionsUIViewControllerMigration is enabled.
// TODO(crbug.com/1285378): remove after completion of UIViewController
// migration.
@property(nonatomic, weak) id<ContentSuggestionsCollectionConsumer>
    collectionConsumer;
@property(nonatomic, weak) id<ContentSuggestionsConsumer> consumer;

// FaviconAttributesProvider to fetch the favicon for the most visited tiles.
@property(nonatomic, strong, readonly)
    FaviconAttributesProvider* mostVisitedAttributesProvider;

// Sets the |mostVisitedData| used to log the impression of the tiles.
- (void)setMostVisitedDataForLogging:
    (const ntp_tiles::NTPTilesVector&)mostVisitedData;

// Fetches the favicon for this |item|.
- (void)fetchFaviconForMostVisited:(ContentSuggestionsMostVisitedItem*)item;
// Fetches the favicon for |item| within |parentItem|.
// TODO(crbug.com/1285378): Remove this after fully migrating ContentSuggestions
// to UIViewController.
- (void)fetchFaviconForMostVisited:(ContentSuggestionsMostVisitedItem*)item
                        parentItem:(ContentSuggestionsParentItem*)parentItem;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_FAVICON_MEDIATOR_H_

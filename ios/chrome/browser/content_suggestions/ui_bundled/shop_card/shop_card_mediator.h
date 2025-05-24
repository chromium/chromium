// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SHOP_CARD_SHOP_CARD_MEDIATOR_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SHOP_CARD_SHOP_CARD_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "components/image_fetcher/core/image_data_fetcher.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/shop_card/shop_card_commands.h"

namespace bookmarks {
class BookmarkModel;
}

namespace commerce {
class ShoppingService;
}

@class ContentSuggestionsMetricsRecorder;
class ImpressionLimitService;
class PrefService;
class FaviconLoader;
@protocol NewTabPageActionsDelegate;
@protocol ShopCardActionDelegate;
@class ShopCardItem;
@class ShopCardData;

// Delegate to communicate events back to owner of ShopCardMediator.
@protocol ShopCardMediatorDelegate

// Remove ShopCard from the magic stack.
- (void)removeShopCard;

// Add ShopCard to the magic stack.
- (void)insertShopCard;

@end

@interface ShopCardMediator : NSObject <ShopCardCommands>

// Default initializer.
- (instancetype)
    initWithShoppingService:(commerce::ShoppingService*)shoppingService
                prefService:(PrefService*)prefService
              bookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
               imageFetcher:
                   (std::unique_ptr<image_fetcher::ImageDataFetcher>)fetcher
              faviconLoader:(FaviconLoader*)faviconLoader
     impressionLimitService:(ImpressionLimitService*)impressionLimitService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

- (void)disconnect;

- (void)reset;

- (void)disableModule;

- (ShopCardItem*)shopCardItemToShow;

// Delegate used to communicate events back to the owner of this class.
@property(nonatomic, weak) id<ShopCardMediatorDelegate> delegate;

// Delegate to communicate events back to the ContentSuggestionsCoordinator.
@property(nonatomic, weak) id<ShopCardActionDelegate> shopCardActionDelegate;

// Delegate for reporting content suggestions actions to the NTP.
@property(nonatomic, weak) id<NewTabPageActionsDelegate> NTPActionsDelegate;

// Recorder for content suggestions metrics.
@property(nonatomic, weak)
    ContentSuggestionsMetricsRecorder* contentSuggestionsMetricsRecorder;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SHOP_CARD_SHOP_CARD_MEDIATOR_H_

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PRICE_NOTIFICATIONS_PRICE_NOTIFICATIONS_PRICE_TRACKING_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_PRICE_NOTIFICATIONS_PRICE_NOTIFICATIONS_PRICE_TRACKING_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import <memory>

#import "base/memory/weak_ptr.h"
#import "ios/chrome/browser/price_insights/ui/price_insights_mutator.h"
#import "ios/chrome/browser/ui/price_notifications/price_notifications_mutator.h"

@protocol BookmarksCommands;
@protocol PriceNotificationsAlertPresenter;
@protocol PriceNotificationsCommands;
@protocol PriceNotificationsConsumer;
@protocol PriceInsightsConsumer;
class PushNotificationService;

namespace bookmarks {
class BookmarkModel;
}  // namespace bookmarks

namespace commerce {
class ShoppingService;
}  // namespace commerce

namespace image_fetcher {
class ImageDataFetcher;
}  // namespace image_fetcher

namespace web {
class WebState;
}  // namespace web

@interface PriceNotificationsPriceTrackingMediator
    : NSObject <PriceNotificationsMutator, PriceInsightsMutator>

// `WebState`, and `PushNotificationService` must not be nil.
// The designated initializer. `ShoppingService`, `BookmarkModel`,
// `ImageDataFetcher`, `WebState`, and `PushNotificationService` must not be
// nil.
- (instancetype)
    initWithShoppingService:(commerce::ShoppingService*)service
              bookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
               imageFetcher:(std::unique_ptr<image_fetcher::ImageDataFetcher>)
                                imageFetcher
                   webState:(base::WeakPtr<web::WebState>)webState
    pushNotificationService:(PushNotificationService*)pushNotificationService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
// The handler that is responsible for navigating the user to the Bookmarks UI.
@property(nonatomic, weak) id<BookmarksCommands> bookmarksHandler;

@property(nonatomic, weak) id<PriceNotificationsConsumer> consumer;

@property(nonatomic, weak) id<PriceInsightsConsumer> priceInsightsConsumer;

@property(nonatomic, weak) id<PriceNotificationsCommands> handler;

@property(nonatomic, weak) id<PriceNotificationsAlertPresenter> presenter;

// The GAIA ID of the user currently signed into Chrome;
@property(nonatomic, copy) NSString* gaiaID;

@end

#endif  // IOS_CHROME_BROWSER_UI_PRICE_NOTIFICATIONS_PRICE_NOTIFICATIONS_PRICE_TRACKING_MEDIATOR_H_

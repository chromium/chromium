// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PRICE_NOTIFICATIONS_PRICE_NOTIFICATIONS_PRICE_TRACKING_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_PRICE_NOTIFICATIONS_PRICE_NOTIFICATIONS_PRICE_TRACKING_MEDIATOR_H_

#import <Foundation/Foundation.h>
#import <memory>

#import "ios/chrome/browser/ui/price_notifications/price_notifications_mutator.h"

@protocol PriceNotificationsConsumer;

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
    : NSObject <PriceNotificationsMutator>

// The designated initializer. `ShoppingService`, `BookmarkModel`,
// `ImageDataFetcher` and `WebState` must not be nil.
- (instancetype)
    initWithShoppingService:(commerce::ShoppingService*)service
              bookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
               imageFetcher:(std::unique_ptr<image_fetcher::ImageDataFetcher>)
                                imageFetcher
                   webState:(web::WebState*)webState NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@property(nonatomic, weak) id<PriceNotificationsConsumer> consumer;

@end

#endif  // IOS_CHROME_BROWSER_UI_PRICE_NOTIFICATIONS_PRICE_NOTIFICATIONS_PRICE_TRACKING_MEDIATOR_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_PRICE_TRACKING_PROMO_PRICE_TRACKING_PROMO_MEDIATOR_TESTING_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_PRICE_TRACKING_PROMO_PRICE_TRACKING_PROMO_MEDIATOR_TESTING_H_

#import "ios/chrome/browser/ui/content_suggestions/price_tracking_promo/price_tracking_promo_mediator.h"

class AuthenticationService;
@class MDCSnackbarMessage;
class NotificationsSettingsObserver;
class PrefService;
@class PriceTrackingPromoItem;
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

// Category for exposing internal state for testing.
@interface PriceTrackingPromoMediator (ForTesting)

- (commerce::ShoppingService*)shoppingServiceForTesting;

- (bookmarks::BookmarkModel*)bookmarkModelForTesting;

- (PrefService*)prefServiceForTesting;

- (PushNotificationService*)pushNotificationServiceForTesting;

- (AuthenticationService*)authenticationServiceForTesting;

- (image_fetcher::ImageDataFetcher*)imageFetcherForTesting;

- (PriceTrackingPromoItem*)priceTrackingPromoItemForTesting;

- (MDCSnackbarMessage*)snackbarMessageForTesting;

- (NotificationsSettingsObserver*)notificationsSettingsObserverForTesting;

- (void)enablePriceTrackingNotificationsSettingsForTesting;

- (void)setPriceTrackingPromoItemForTesting:(PriceTrackingPromoItem*)item;

- (void)requestPushNotificationDoneWithGrantedForTesting:(BOOL)granted
                                             promptShown:(BOOL)promptShown
                                                   error:(NSError*)error;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_PRICE_TRACKING_PROMO_PRICE_TRACKING_PROMO_MEDIATOR_TESTING_H_

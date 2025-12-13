// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_PRICE_TRACKING_PROMO_COORDINATOR_PRICE_TRACKING_PROMO_MEDIATOR_TESTING_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_PRICE_TRACKING_PROMO_COORDINATOR_PRICE_TRACKING_PROMO_MEDIATOR_TESTING_H_

#import "ios/chrome/browser/content_suggestions/ui_bundled/price_tracking_promo/coordinator/price_tracking_promo_mediator.h"

class AuthenticationService;
class NotificationsSettingsObserver;
class PrefService;
@class PriceTrackingPromoItem;
class PushNotificationService;
@class SnackbarMessage;

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

- (SnackbarMessage*)snackbarMessageForTesting;

- (NotificationsSettingsObserver*)notificationsSettingsObserverForTesting;

- (FaviconLoader*)faviconLoaderForTesting;

- (void)enablePriceTrackingNotificationsSettingsForTesting;

- (void)setPriceTrackingPromoItemForTesting:(PriceTrackingPromoItem*)item;

- (void)requestPushNotificationDoneWithGrantedForTesting:(BOOL)granted
                                             promptShown:(BOOL)promptShown
                                                   error:(NSError*)error;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_PRICE_TRACKING_PROMO_COORDINATOR_PRICE_TRACKING_PROMO_MEDIATOR_TESTING_H_

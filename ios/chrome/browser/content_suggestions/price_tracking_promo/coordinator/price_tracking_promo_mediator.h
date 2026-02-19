// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_PRICE_TRACKING_PROMO_COORDINATOR_PRICE_TRACKING_PROMO_MEDIATOR_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_PRICE_TRACKING_PROMO_COORDINATOR_PRICE_TRACKING_PROMO_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "components/image_fetcher/core/image_data_fetcher.h"
#import "ios/chrome/browser/content_suggestions/price_tracking_promo/ui/price_tracking_promo_commands.h"

namespace bookmarks {
class BookmarkModel;
}  // namespace bookmarks

namespace commerce {
class ShoppingService;
}  // namespace commerce

class AuthenticationService;
class FaviconLoader;
@protocol NewTabPageActionsDelegate;
class PrefService;
@protocol PriceTrackingPromoActionDelegate;
@class PriceTrackingPromoConfig;
@protocol PriceTrackingPromoMediatorDelegate;
class PushNotificationService;
@protocol SceneCommands;
@protocol SnackbarCommands;

// Mediator for the Price Tracking Promo card in the Magic Stack.
@interface PriceTrackingPromoMediator : NSObject <PriceTrackingPromoCommands>

// Delegate used to communicate events back to the owner of this class.
@property(nonatomic, weak) id<PriceTrackingPromoMediatorDelegate> delegate;

// Dispatcher.
@property(nonatomic, weak) id<SceneCommands, SnackbarCommands> dispatcher;

// Delegate to delegate actions to the owner of the PriceTrackingPromoMediator
@property(nonatomic, weak) id<PriceTrackingPromoActionDelegate> actionDelegate;

// Delegate for reporting content suggestions actions to the NTP.
@property(nonatomic, weak) id<NewTabPageActionsDelegate> NTPActionsDelegate;

// Default initializer.
- (instancetype)
    initWithShoppingService:(commerce::ShoppingService*)shoppingService
              bookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
               imageFetcher:
                   (std::unique_ptr<image_fetcher::ImageDataFetcher>)fetcher
                prefService:(PrefService*)prefService
                 localState:(PrefService*)localState
    pushNotificationService:(PushNotificationService*)pushNotificationService
      authenticationService:(AuthenticationService*)authenticationService
              faviconLoader:(FaviconLoader*)faviconLoader
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects this mediator.
- (void)disconnect;

// Resets the latest fetched subscriptions and re-fetches if applicable.
- (void)reset;

// Fetches the most recent subscription for the user.
- (void)fetchLatestSubscription;

// Disables and hides the price tracking promo module.
- (void)disableModule;

// Data for price tracking promo to show. Includes the image for the
// latest subscription to be displayed.
- (PriceTrackingPromoConfig*)priceTrackingPromoConfigToShow;

// Enable price tracking notifications settings and show
// snackbar giving user the option to manage these settings.
- (void)enablePriceTrackingSettingsAndShowSnackbar;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_PRICE_TRACKING_PROMO_COORDINATOR_PRICE_TRACKING_PROMO_MEDIATOR_H_

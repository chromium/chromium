// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_PRICE_TRACKING_PROMO_PRICE_TRACKING_PROMO_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_PRICE_TRACKING_PROMO_PRICE_TRACKING_PROMO_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "components/image_fetcher/core/image_data_fetcher.h"
#import "ios/chrome/browser/ui/content_suggestions/price_tracking_promo/price_tracking_promo_commands.h"

namespace bookmarks {
class BookmarkModel;
}  // namespace bookmarks

namespace commerce {
class ShoppingService;
}

@protocol ApplicationCommands;
@protocol NewTabPageActionsDelegate;
class PrefService;
@class PriceTrackingPromoItem;
@protocol PriceTrackingPromoActionDelegate;
class PushNotificationService;
@protocol SnackbarCommands;
@protocol SystemIdentity;

class AuthenticationService;

// Delegate used to communicate events back to the owner of
// PriceTrackingPromoMediator.
@protocol PriceTrackingPromoMediatorDelegate

// New subscription for user observed (originating from a different platform).
- (void)newSubscriptionAvailable;

// Price Tracking Promo is removed from the magic stack.
- (void)removePriceTrackingPromo;

@end

@interface PriceTrackingPromoMediator : NSObject <PriceTrackingPromoCommands>

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
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

- (void)disconnect;

// Resets the latest fetched subscriptions and re-fetches if applicable.
- (void)reset;

// Fetches the most recent subscription for the user.
- (void)fetchLatestSubscription;

// Disables and hides the price tracking promo module.
- (void)disableModule;

// Data for price tracking promo to show. Includes the image for the
// latest subscription to be displayed.
- (PriceTrackingPromoItem*)priceTrackingPromoItemToShow;

// Remove price tracking promo from magic stack
- (void)removePriceTrackingPromo;

// Enable price tracking notifications settings and show
// snackbar giving user the option to manage these settings.
- (void)enablePriceTrackingSettingsAndShowSnackbar;

// Delegate used to communicate events back to the owner of this class.
@property(nonatomic, weak) id<PriceTrackingPromoMediatorDelegate> delegate;

// Dispatcher.
@property(nonatomic, weak) id<ApplicationCommands, SnackbarCommands> dispatcher;

// Delegate to delegate actions to the owner of the PriceTrackingPromoMediator
@property(nonatomic, weak) id<PriceTrackingPromoActionDelegate> actionDelegate;

// Delegate for reporting content suggestions actions to the NTP.
@property(nonatomic, weak) id<NewTabPageActionsDelegate> NTPActionsDelegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_PRICE_TRACKING_PROMO_PRICE_TRACKING_PROMO_MEDIATOR_H_

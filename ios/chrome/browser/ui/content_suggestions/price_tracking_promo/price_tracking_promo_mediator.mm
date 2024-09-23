// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/price_tracking_promo/price_tracking_promo_mediator.h"

#import <MaterialComponents/MaterialSnackbar.h>

#import "base/memory/raw_ptr.h"
#import "components/commerce/core/pref_names.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/push_notification/model/push_notification_service.h"
#import "ios/chrome/browser/push_notification/model/push_notification_util.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/ui/util/snackbar_util.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/ui/content_suggestions/price_tracking_promo/price_tracking_promo_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/price_tracking_promo/price_tracking_promo_item.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "ui/base/l10n/l10n_util.h"

@implementation PriceTrackingPromoMediator {
  raw_ptr<commerce::ShoppingService> _shoppingService;
  PriceTrackingPromoItem* _priceTrackingPromoItem;
  raw_ptr<PrefService> _prefService;
  raw_ptr<PushNotificationService> _pushNotificationService;
  raw_ptr<AuthenticationService> _authenticationService;
}

- (instancetype)
    initWithShoppingService:(commerce::ShoppingService*)shoppingService
                prefService:(PrefService*)prefService
    pushNotificationService:(PushNotificationService*)pushNotificationService
      authenticationService:(AuthenticationService*)authenticationService {
  self = [super init];
  if (self) {
    _shoppingService = shoppingService;
    _prefService = prefService;
    _pushNotificationService = pushNotificationService;
    _authenticationService = authenticationService;
    // _priceTrackingPromoItem will ultimately be filled with data
    // fetched via ShoppingService. However, for now use a blank object
    // to hold the magic stack integration together and enable magic
    // stack card to be build out with static assets. This will
    // be removed when TODO(crbug.com/361106168) is implemented.
    _priceTrackingPromoItem = [[PriceTrackingPromoItem alloc] init];
    _priceTrackingPromoItem.commandHandler = self;
  }
  return self;
}

- (void)disconnect {
  _shoppingService = nil;
}

- (void)reset {
  _priceTrackingPromoItem = nil;
}

- (void)fetchLatestSubscription {
  // TODO(crbug.com/361405189) fetch latest subscription and
  // convert to PriceTrackingPromoItem.
}

- (PriceTrackingPromoItem*)priceTrackingPromoItemToShow {
  return _priceTrackingPromoItem;
}

#pragma mark - Public

- (void)disableModule {
  // TODO(crbug.com/361404422) implement response to
  // user choosing to disable module.
}

#pragma mark - PriceTrackingPromoCommands

- (void)allowPriceTrackingNotifications {
  __weak PriceTrackingPromoMediator* weakSelf = self;
  [PushNotificationUtil requestPushNotificationPermission:^(
                            BOOL granted, BOOL promptShown, NSError* error) {
    web::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](__typeof(self) strongSelf, BOOL granted, NSError* error) {
              [strongSelf requestPushNotificationDoneWithGranted:granted
                                                           error:error];
            },
            weakSelf, granted, error));
  }];
  // TODO(crbug.com/361107641) implement opt in flow C
}

#pragma mark - Private

- (void)requestPushNotificationDoneWithGranted:(BOOL)granted
                                         error:(NSError*)error {
  if (granted && !error) {
    [self enablePriceTrackingNotificationsSettings];
    [self.dispatcher showSnackbarMessage:[self snackbarMessage]];
  }
  [self.delegate removePriceTrackingPromo];
}

// Enable push notifications and email notifications for price tracking.
- (void)enablePriceTrackingNotificationsSettings {
  _prefService->SetBoolean(commerce::kPriceEmailNotificationsEnabled, true);
  // TODO(crbug.com/368005246) Ensure sign out flows are handled correctly.
  id<SystemIdentity> identity =
      _authenticationService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  _pushNotificationService->SetPreference(
      identity.gaiaID, PushNotificationClientId::kCommerce, true);
}

// Get snackbar indicating price tracking notifications are enabled with
// an action to go to settings to toggle either.
- (MDCSnackbarMessage*)snackbarMessage {
  MDCSnackbarMessageAction* action = [[MDCSnackbarMessageAction alloc] init];

  action.handler = ^{
    [self.dispatcher showPriceTrackingNotificationsSettings];
  };
  action.title = l10n_util::GetNSString(
      IDS_IOS_CONTENT_SUGGESTIONS_PRICE_TRACKING_PROMO_SNACKBAR_MANAGE);
  action.accessibilityIdentifier = kPriceTrackingSettingsAccessibilityID;

  MDCSnackbarMessage* message = CreateSnackbarMessage(l10n_util::GetNSString(
      IDS_IOS_CONTENT_SUGGESTIONS_PRICE_TRACKING_PROMO_SNACKBAR_TITLE));
  message.action = action;
  message.category = kPriceTrackingSnackbarCategory;
  return message;
}

// TODO(crbug.com/368064027) move all magic stack clients
// ForTesting to @implementation.
#pragma mark - Testing category methods

- (commerce::ShoppingService*)shoppingServiceForTesting {
  return self->_shoppingService;
}

- (PriceTrackingPromoItem*)priceTrackingPromoItemForTesting {
  return self->_priceTrackingPromoItem;
}

- (MDCSnackbarMessage*)snackbarMessageForTesting {
  return [self snackbarMessage];
}

- (void)enablePriceTrackingNotificationsSettingsForTesting {
  [self enablePriceTrackingNotificationsSettings];
}

@end

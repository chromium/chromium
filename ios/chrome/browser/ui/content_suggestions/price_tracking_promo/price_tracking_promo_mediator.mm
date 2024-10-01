// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/price_tracking_promo/price_tracking_promo_mediator.h"

#import <MaterialComponents/MaterialSnackbar.h>

#import "base/cancelable_callback.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/browser/bookmark_node.h"
#import "components/commerce/core/pref_names.h"
#import "components/commerce/core/price_tracking_utils.h"
#import "components/commerce/core/shopping_service.h"
#import "components/power_bookmarks/core/power_bookmark_utils.h"
#import "components/power_bookmarks/core/proto/power_bookmark_meta.pb.h"
#import "components/power_bookmarks/core/proto/shopping_specifics.pb.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/push_notification/model/push_notification_service.h"
#import "ios/chrome/browser/push_notification/model/push_notification_util.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/ui/util/snackbar_util.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/ui/content_suggestions/price_tracking_promo/price_tracking_promo_action_delegate.h"
#import "ios/chrome/browser/ui/content_suggestions/price_tracking_promo/price_tracking_promo_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/price_tracking_promo/price_tracking_promo_item.h"
#import "ios/chrome/browser/ui/content_suggestions/price_tracking_promo/price_tracking_promo_prefs.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "ui/base/l10n/l10n_util.h"

@implementation PriceTrackingPromoMediator {
  raw_ptr<commerce::ShoppingService> _shoppingService;
  raw_ptr<bookmarks::BookmarkModel> _bookmarkModel;
  PriceTrackingPromoItem* _priceTrackingPromoItem;
  raw_ptr<PrefService> _prefService;
  raw_ptr<PushNotificationService> _pushNotificationService;
  raw_ptr<AuthenticationService> _authenticationService;
  std::unique_ptr<image_fetcher::ImageDataFetcher> _imageFetcher;
}

- (instancetype)
    initWithShoppingService:(commerce::ShoppingService*)shoppingService
              bookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
               imageFetcher:
                   (std::unique_ptr<image_fetcher::ImageDataFetcher>)fetcher
                prefService:(PrefService*)prefService
    pushNotificationService:(PushNotificationService*)pushNotificationService
      authenticationService:(AuthenticationService*)authenticationService {
  self = [super init];
  if (self) {
    _shoppingService = shoppingService;
    _bookmarkModel = bookmarkModel;
    _imageFetcher = std::move(fetcher);
    _prefService = prefService;
    _pushNotificationService = pushNotificationService;
    _authenticationService = authenticationService;
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
  if (self->_priceTrackingPromoItem) {
    return;
  }
  __weak PriceTrackingPromoMediator* weakSelf = self;
  GetAllPriceTrackedBookmarks(
      _shoppingService, _bookmarkModel,
      base::BindOnce(
          ^(std::vector<const bookmarks::BookmarkNode*> subscriptions) {
            PriceTrackingPromoMediator* strongSelf = weakSelf;
            if (!strongSelf || !strongSelf.delegate) {
              return;
            }
            [strongSelf onPriceTrackedBookarksReceived:subscriptions];
          }));
}

- (PriceTrackingPromoItem*)priceTrackingPromoItemToShow {
  return _priceTrackingPromoItem;
}

- (void)removePriceTrackingPromo {
  [self.delegate removePriceTrackingPromo];
}

- (void)enablePriceTrackingSettingsAndShowSnackbar {
  [self enablePriceTrackingNotificationsSettings];
  [self.dispatcher showSnackbarMessage:[self snackbarMessage]];
}

#pragma mark - Public

- (void)disableModule {
  _prefService->SetBoolean(kPriceTrackingPromoDisabled, true);
  [self.delegate removePriceTrackingPromo];
}

- (void)setDelegate:(id<PriceTrackingPromoMediatorDelegate>)delegate {
  _delegate = delegate;
  if (_delegate) {
    [self fetchLatestSubscription];
  }
}

#pragma mark - PriceTrackingPromoCommands

- (void)allowPriceTrackingNotifications {
  __weak PriceTrackingPromoMediator* weakSelf = self;
  [PushNotificationUtil requestPushNotificationPermission:^(
                            BOOL granted, BOOL promptShown, NSError* error) {
    web::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(
                       [](__typeof(self) strongSelf, BOOL granted,
                          BOOL promptShown, NSError* error) {
                         [strongSelf
                             requestPushNotificationDoneWithGranted:granted
                                                        promptShown:promptShown
                                                              error:error];
                       },
                       weakSelf, granted, promptShown, error));
  }];
}

#pragma mark - Private

- (void)requestPushNotificationDoneWithGranted:(BOOL)granted
                                   promptShown:(BOOL)promptShown
                                         error:(NSError*)error {
  if (granted && !error) {
    [self enablePriceTrackingNotificationsSettings];
    [self.dispatcher showSnackbarMessage:[self snackbarMessage]];
    [self.delegate removePriceTrackingPromo];
  } else if (!granted && !promptShown && !error) {
    [self.actionDelegate showPriceTrackingPromoAlertCoordinator];
  }
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

- (void)onPriceTrackedBookarksReceived:
    (std::vector<const bookmarks::BookmarkNode*>)subscriptions {
  const power_bookmarks::ShoppingSpecifics* most_recent =
      [self getMostRecentBookmarkSpecifics:subscriptions];
  if (!most_recent) {
    return;
  }
  __weak PriceTrackingPromoMediator* weakSelf = self;
  // There is a subscription but no image url - the price tracking promo
  // will be displayed but with the fallback image.
  if (!most_recent->has_image_url()) {
    _priceTrackingPromoItem = [[PriceTrackingPromoItem alloc] init];
    _priceTrackingPromoItem.commandHandler = self;
    [self.delegate newSubscriptionAvailable];
  } else {
    // If we have an image, fetch it and display the price tracking promo
    // with that image.
    _imageFetcher->FetchImageData(
        GURL(most_recent->image_url()),
        base::BindOnce(^(const std::string& imageData,
                         const image_fetcher::RequestMetadata& metadata) {
          PriceTrackingPromoMediator* strongSelf = weakSelf;
          if (!strongSelf || !strongSelf.delegate) {
            return;
          }
          [strongSelf onImageFetchedResult:imageData];
        }),
        NO_TRAFFIC_ANNOTATION_YET);
  }
}

- (const power_bookmarks::ShoppingSpecifics*)getMostRecentBookmarkSpecifics:
    (std::vector<const bookmarks::BookmarkNode*>)subscriptions {
  const power_bookmarks::ShoppingSpecifics* most_recent = nil;
  for (const bookmarks::BookmarkNode* bookmark : subscriptions) {
    std::unique_ptr<power_bookmarks::PowerBookmarkMeta> meta =
        power_bookmarks::GetNodePowerBookmarkMeta(_bookmarkModel, bookmark);
    if (!meta || !meta->has_shopping_specifics()) {
      continue;
    }
    const power_bookmarks::ShoppingSpecifics specifics =
        meta->shopping_specifics();
    if (!most_recent || specifics.last_subscription_change_time() >
                            most_recent->last_subscription_change_time()) {
      most_recent = &specifics;
    }
  }
  return most_recent;
}

- (void)onImageFetchedResult:(const std::string&)imageData {
  self->_priceTrackingPromoItem = [[PriceTrackingPromoItem alloc] init];
  self->_priceTrackingPromoItem.commandHandler = self;
  NSData* data = [NSData dataWithBytes:imageData.data()
                                length:imageData.size()];
  if (data) {
    self->_priceTrackingPromoItem.productImageData = data;
  }
  [self.delegate newSubscriptionAvailable];
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

- (void)setPriceTrackingPromoItemForTesting:(PriceTrackingPromoItem*)item {
  self->_priceTrackingPromoItem = item;
}

@end

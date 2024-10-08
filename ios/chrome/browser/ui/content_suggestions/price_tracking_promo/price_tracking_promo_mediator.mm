// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/price_tracking_promo/price_tracking_promo_mediator.h"

#import <MaterialComponents/MaterialSnackbar.h>

#import "base/cancelable_callback.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/browser/bookmark_node.h"
#import "components/commerce/core/pref_names.h"
#import "components/commerce/core/price_tracking_utils.h"
#import "components/commerce/core/shopping_service.h"
#import "components/power_bookmarks/core/power_bookmark_utils.h"
#import "components/power_bookmarks/core/proto/power_bookmark_meta.pb.h"
#import "components/power_bookmarks/core/proto/shopping_specifics.pb.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_actions_delegate.h"
#import "ios/chrome/browser/push_notification/model/provisional_push_notification_util.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/push_notification/model/push_notification_service.h"
#import "ios/chrome/browser/push_notification/model/push_notification_settings_util.h"
#import "ios/chrome/browser/push_notification/model/push_notification_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/ui/util/snackbar_util.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/ui/content_suggestions/price_tracking_promo/price_tracking_promo_action_delegate.h"
#import "ios/chrome/browser/ui/content_suggestions/price_tracking_promo/price_tracking_promo_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/price_tracking_promo/price_tracking_promo_item.h"
#import "ios/chrome/browser/ui/content_suggestions/price_tracking_promo/price_tracking_promo_prefs.h"
#import "ios/chrome/browser/ui/settings/notifications/notifications_settings_observer.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Enum corresponding to enums.xml's PriceTrackingPromoOptInFlow.
// Entries should not be renumbered and numeric values should never be reused.
enum class PriceTrackingPromoOptInFlow {
  // User has never used notifications in the app before.
  kFirstTime = 0,
  // User has notifications turned on, but not for price tracking.
  kEnablePriceTracking = 1,
  // User has notifications turned off and will be prompted to re-enable them.
  kReenableNotifications = 2,
  kMaxValue = kReenableNotifications,
};

void LogOptInFlowHistogram(PriceTrackingPromoOptInFlow opt_in_flow) {
  base::UmaHistogramEnumeration(
      "Commerce.PriceTracking.MagicStackPromo.OptInFlow", opt_in_flow);
}

}  // namespace

@interface PriceTrackingPromoMediator () <NotificationsSettingsObserverDelegate,
                                          PrefObserverDelegate>
@end

@implementation PriceTrackingPromoMediator {
  raw_ptr<commerce::ShoppingService> _shoppingService;
  raw_ptr<bookmarks::BookmarkModel> _bookmarkModel;
  PriceTrackingPromoItem* _priceTrackingPromoItem;
  raw_ptr<PrefService> _prefService;
  raw_ptr<PushNotificationService> _pushNotificationService;
  raw_ptr<AuthenticationService> _authenticationService;
  std::unique_ptr<image_fetcher::ImageDataFetcher> _imageFetcher;
  NotificationsSettingsObserver* _notificationsObserver;
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  PrefChangeRegistrar _prefChangeRegistrar;
}

- (instancetype)
    initWithShoppingService:(commerce::ShoppingService*)shoppingService
              bookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
               imageFetcher:
                   (std::unique_ptr<image_fetcher::ImageDataFetcher>)fetcher
                prefService:(PrefService*)prefService
                 localState:(PrefService*)localState
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
    _notificationsObserver =
        [[NotificationsSettingsObserver alloc] initWithPrefService:_prefService
                                                        localState:localState];

    _notificationsObserver.delegate = self;
    _prefObserverBridge = std::make_unique<PrefObserverBridge>(self);
    _prefChangeRegistrar.Init(prefService);
    _prefObserverBridge->ObserveChangesForPreference(
        kPriceTrackingPromoDisabled, &_prefChangeRegistrar);
  }
  return self;
}

- (void)disconnect {
  _shoppingService = nil;
  _bookmarkModel = nil;
  _prefService = nil;
  _pushNotificationService = nil;
  _authenticationService = nil;
  _imageFetcher = nil;
  _notificationsObserver.delegate = nil;
  [_notificationsObserver disconnect];
  _notificationsObserver = nil;
  _prefChangeRegistrar.RemoveAll();
  _prefObserverBridge.reset();
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

// TODO(crbug.com/371870438) merge disableModule &&
// removePriceTrackingPromo. They are the same (the price tracking
// promo can only be displayed once).
- (void)removePriceTrackingPromo {
  [self disableModule];
}

- (void)enablePriceTrackingSettingsAndShowSnackbar {
  [self enablePriceTrackingNotificationsSettings];
  [self.dispatcher showSnackbarMessage:[self snackbarMessage]];
}

#pragma mark - Public

- (void)disableModule {
  _prefService->SetBoolean(kPriceTrackingPromoDisabled, true);
}

- (void)setDelegate:(id<PriceTrackingPromoMediatorDelegate>)delegate {
  _delegate = delegate;
  if (_delegate) {
    [self fetchLatestSubscription];
  }
}

#pragma mark - PriceTrackingPromoCommands

- (void)allowPriceTrackingNotifications {
  base::RecordAction(
      base::UserMetricsAction("Commerce.PriceTracking.MagicStackPromo.Allow"));
  [self.NTPActionsDelegate priceTrackingPromoOpened];
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

#pragma mark - NotificationsSettingsObserverDelegate

- (void)notificationsSettingsDidChangeForClient:
    (PushNotificationClientId)clientID {
  if (clientID == PushNotificationClientId::kCommerce) {
    id<SystemIdentity> identity = _authenticationService->GetPrimaryIdentity(
        signin::ConsentLevel::kSignin);
    if (push_notification_settings::
            GetMobileNotificationPermissionStatusForClient(
                PushNotificationClientId::kCommerce,
                base::SysNSStringToUTF8(identity.gaiaID))) {
      [self disableModule];
    }
  }
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (preferenceName == kPriceTrackingPromoDisabled &&
      _prefService->GetBoolean(kPriceTrackingPromoDisabled)) {
    [self.delegate removePriceTrackingPromo];
  }
}

#pragma mark - Private

- (void)requestPushNotificationDoneWithGranted:(BOOL)granted
                                   promptShown:(BOOL)promptShown
                                         error:(NSError*)error {
  // If prompt was shown, it is the first time the user has
  // used notifications in the app.
  if (promptShown) {
    LogOptInFlowHistogram(PriceTrackingPromoOptInFlow::kFirstTime);
    if (granted) {
      base::RecordAction(base::UserMetricsAction(
          "Commerce.PriceTracking.MagicStackPromo.FirstTime.Allow"));
    } else {
      base::RecordAction(base::UserMetricsAction(
          "Commerce.PriceTracking.MagicStackPromo.FirstTime.Deny"));
    }
  } else if (granted) {
    // If the prompt wasn't shown but permissions are granted, the
    // user has previously enabled notifications in the app.
    LogOptInFlowHistogram(PriceTrackingPromoOptInFlow::kEnablePriceTracking);
  }

  if (granted && !error) {
    [self enablePriceTrackingNotificationsSettings];
    [self.dispatcher showSnackbarMessage:[self snackbarMessage]];
    [self disableModule];
  } else if (!granted && !promptShown && !error) {
    // If the prompt wasn't shown and permission is denied, the user
    // has turned off notifications in the app before.
    LogOptInFlowHistogram(PriceTrackingPromoOptInFlow::kReenableNotifications);
    [self.actionDelegate showPriceTrackingPromoAlertCoordinator];
  } else {
    // Catch all other scenarios e.g. first time opt in and user
    // denied access to notifications, should remove and disable module.
    [self disableModule];
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
  if (subscriptions.empty()) {
    return;
  }
  GURL most_recent_subscription_product_url =
      [self getMostRecentSubscriptionProductUrl:subscriptions];
  __weak PriceTrackingPromoMediator* weakSelf = self;
  // There is a subscription but no image url - the price tracking promo
  // will be displayed but with the fallback image.
  if (most_recent_subscription_product_url.is_empty()) {
    _priceTrackingPromoItem = [[PriceTrackingPromoItem alloc] init];
    _priceTrackingPromoItem.commandHandler = self;
    [self.delegate newSubscriptionAvailable];
  } else {
    // If we have an image, fetch it and display the price tracking promo
    // with that image.
    _imageFetcher->FetchImageData(
        most_recent_subscription_product_url,
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

- (GURL)getMostRecentSubscriptionProductUrl:
    (std::vector<const bookmarks::BookmarkNode*>)subscriptions {
  GURL most_recent_subscription_product_url;
  int64_t most_recent_subscription_time = 0;
  for (const bookmarks::BookmarkNode* bookmark : subscriptions) {
    std::unique_ptr<power_bookmarks::PowerBookmarkMeta> meta =
        power_bookmarks::GetNodePowerBookmarkMeta(_bookmarkModel, bookmark);
    if (!meta || !meta->has_shopping_specifics()) {
      continue;
    }
    const power_bookmarks::ShoppingSpecifics specifics =
        meta->shopping_specifics();
    if (most_recent_subscription_product_url.is_empty() ||
        most_recent_subscription_time <
            specifics.last_subscription_change_time()) {
      most_recent_subscription_product_url = GURL(meta->lead_image().url());
      most_recent_subscription_time = specifics.last_subscription_change_time();
    }
  }
  return most_recent_subscription_product_url;
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

- (bookmarks::BookmarkModel*)bookmarkModelForTesting {
  return self->_bookmarkModel;
}

- (PrefService*)prefServiceForTesting {
  return self->_prefService;
}

- (PushNotificationService*)pushNotificationServiceForTesting {
  return self->_pushNotificationService;
}

- (AuthenticationService*)authenticationServiceForTesting {
  return self->_authenticationService;
}

- (image_fetcher::ImageDataFetcher*)imageFetcherForTesting {
  return self->_imageFetcher.get();
}

- (PriceTrackingPromoItem*)priceTrackingPromoItemForTesting {
  return self->_priceTrackingPromoItem;
}

- (MDCSnackbarMessage*)snackbarMessageForTesting {
  return [self snackbarMessage];
}

- (NotificationsSettingsObserver*)notificationsSettingsObserverForTesting {
  return self->_notificationsObserver;
}

- (void)enablePriceTrackingNotificationsSettingsForTesting {
  [self enablePriceTrackingNotificationsSettings];
}

- (void)setPriceTrackingPromoItemForTesting:(PriceTrackingPromoItem*)item {
  self->_priceTrackingPromoItem = item;
}

- (void)requestPushNotificationDoneWithGrantedForTesting:(BOOL)granted
                                             promptShown:(BOOL)promptShown
                                                   error:(NSError*)error {
  [self requestPushNotificationDoneWithGranted:granted
                                   promptShown:promptShown
                                         error:error];
}

@end

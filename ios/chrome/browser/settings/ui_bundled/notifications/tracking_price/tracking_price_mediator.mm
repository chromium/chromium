// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/notifications/tracking_price/tracking_price_mediator.h"

#import "base/apple/foundation_util.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/commerce/core/pref_names.h"
#import "components/commerce/core/price_tracking_utils.h"
#import "components/commerce/core/shopping_service.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/push_notification/model/push_notification_account_context_manager.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/push_notification/model/push_notification_profile_service.h"
#import "ios/chrome/browser/push_notification/model/push_notification_profile_service_factory.h"
#import "ios/chrome/browser/push_notification/model/push_notification_service.h"
#import "ios/chrome/browser/push_notification/model/push_notification_settings_util.h"
#import "ios/chrome/browser/push_notification/model/push_notification_util.h"
#import "ios/chrome/browser/settings/ui_bundled/notifications/tracking_price/tracking_price_alert_presenter.h"
#import "ios/chrome/browser/settings/ui_bundled/notifications/tracking_price/tracking_price_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/notifications/tracking_price/tracking_price_consumer.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/utils/observable_boolean.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

// List of items.
typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeMobileNotifications = kItemTypeEnumZero,
  ItemTypeTrackPriceHeader,
  ItemTypeEmailNotifications,
};

@interface TrackingPriceMediator () <BooleanObserver, PrefObserverDelegate> {
  // Pref observer to track changes to prefs.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  PrefChangeRegistrar _prefChangeRegistrar;
  // A boolean associated with a pref that is used to observe its changes.
  PrefBackedBoolean* _emailNotificationsEnabled;
}

// Header item.
@property(nonatomic, strong)
    TableViewLinkHeaderFooterItem* trackPriceHeaderItem;

// The service responsible for interacting with commerce's price data
// infrastructure.
@property(nonatomic, assign) commerce::ShoppingService* shoppingService;

// Responsible for retrieving the relevant information about the currently
// signed-in user.
@property(nonatomic, assign) AuthenticationService* authService;

// Pref service to retrieve preference values.
@property(nonatomic, assign) PrefService* prefService;

@end

@implementation TrackingPriceMediator {
  // Identity object that contains the user's account details.
  id<SystemIdentity> _identity;
}

- (instancetype)
    initWithShoppingService:(commerce::ShoppingService*)shoppingService
      authenticationService:(AuthenticationService*)authenticationService
                prefService:(PrefService*)prefService {
  self = [super init];
  if (self) {
    DCHECK(shoppingService);
    DCHECK(authenticationService);
    DCHECK(prefService);
    _shoppingService = shoppingService;
    _authService = authenticationService;
    _prefService = prefService;
    _shoppingService->FetchPriceEmailPref();
    _identity = _authService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);

    _prefChangeRegistrar.Init(prefService);
    _emailNotificationsEnabled = [[PrefBackedBoolean alloc]
        initWithPrefService:_prefService
                   prefName:commerce::kPriceEmailNotificationsEnabled];
    [_emailNotificationsEnabled setObserver:self];
    _prefObserverBridge.reset(new PrefObserverBridge(self));
    // Register to observe any changes on Perf backed values displayed by the
    // screen.
    _prefObserverBridge->ObserveChangesForPreference(
        prefs::kFeaturePushNotificationPermissions, &_prefChangeRegistrar);
  }

  return self;
}

- (void)disconnect {
  // Remove pref changes registrations.
  _prefChangeRegistrar.RemoveAll();

  // Remove observer bridges.
  _prefObserverBridge.reset();

  [_emailNotificationsEnabled stop];
  [_emailNotificationsEnabled setObserver:nil];
  _emailNotificationsEnabled = nil;

  self.shoppingService = nullptr;
  self.authService = nullptr;
  self.prefService = nullptr;
  _identity = nil;
}

#pragma mark - Properties

- (TableViewSwitchItem*)mobileNotificationItem {
  if (!_mobileNotificationItem) {
    _mobileNotificationItem =
        [[TableViewSwitchItem alloc] initWithType:ItemTypeMobileNotifications];
    _mobileNotificationItem.text = l10n_util::GetNSString(
        IDS_IOS_TRACKING_PRICE_MOBILE_NOTIFICATIONS_TITLE);
    _mobileNotificationItem.accessibilityIdentifier =
        kSettingsTrackingPriceMobileNotificationsCellId;
    _mobileNotificationItem.on = push_notification_settings::
        GetMobileNotificationPermissionStatusForClient(
            PushNotificationClientId::kCommerce, _identity.gaiaId);
    _mobileNotificationItem.target = self;
    _mobileNotificationItem.selector =
        @selector(mobileNotificationSwitchToggled:);
  }

  return _mobileNotificationItem;
}

- (TableViewSwitchItem*)emailNotificationItem {
  if (!_emailNotificationItem) {
    _emailNotificationItem =
        [[TableViewSwitchItem alloc] initWithType:ItemTypeEmailNotifications];
    _emailNotificationItem.text = l10n_util::GetNSString(
        IDS_IOS_TRACKING_PRICE_EMAIL_NOTIFICATIONS_TITLE);
    _emailNotificationItem.detailText = l10n_util::GetNSStringF(
        IDS_IOS_TRACKING_PRICE_EMAIL_NOTIFICATIONS_DETAILS,
        base::SysNSStringToUTF16(_identity.userEmail));
    _emailNotificationItem.accessibilityIdentifier =
        kSettingsTrackingPriceEmailNotificationsCellId;
    _emailNotificationItem.on =
        _prefService->GetBoolean(commerce::kPriceEmailNotificationsEnabled);
    _emailNotificationItem.target = self;
    _emailNotificationItem.selector =
        @selector(emailNotificationsSwitchToggled:);
  }

  return _emailNotificationItem;
}

- (TableViewLinkHeaderFooterItem*)trackPriceHeaderItem {
  if (!_trackPriceHeaderItem) {
    _trackPriceHeaderItem = [[TableViewLinkHeaderFooterItem alloc]
        initWithType:ItemTypeTrackPriceHeader];
    _trackPriceHeaderItem.text =
        l10n_util::GetNSString(IDS_IOS_TRACKING_PRICE_HEADER_TEXT);
  }

  return _trackPriceHeaderItem;
}

- (void)setConsumer:(id<TrackingPriceConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }
  _consumer = consumer;
  [_consumer setMobileNotificationItem:self.mobileNotificationItem];
  [_consumer setEmailNotificationItem:self.emailNotificationItem];
  [_consumer setTrackPriceHeaderItem:self.trackPriceHeaderItem];
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  if (observableBoolean == _emailNotificationsEnabled) {
    self.emailNotificationItem.on = [_emailNotificationsEnabled value];
    [self.consumer setEmailNotificationItem:self.emailNotificationItem];
  }
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  self.mobileNotificationItem.on = push_notification_settings::
      GetMobileNotificationPermissionStatusForClient(
          PushNotificationClientId::kCommerce, _identity.gaiaId);
  [self.consumer setMobileNotificationItem:self.mobileNotificationItem];
}

#pragma mark - Private

// Updates the current user's permission preference for the given `client_id`.
- (void)setPreferenceFor:(PushNotificationClientId)clientID to:(BOOL)enabled {
  PushNotificationService* service =
      GetApplicationContext()->GetPushNotificationService();
  service->SetPreference(_identity.gaiaId, clientID, enabled);
}

- (void)mobileNotificationSwitchToggled:(UISwitch*)sender {
  [self setPreferenceFor:PushNotificationClientId::kCommerce to:sender.on];
  self.mobileNotificationItem.on = push_notification_settings::
      GetMobileNotificationPermissionStatusForClient(
          PushNotificationClientId::kCommerce, GaiaId(_identity.gaiaId));
  if (!sender.on) {
    return;
  }

  __weak TrackingPriceMediator* weakSelf = self;
  [PushNotificationUtil requestPushNotificationPermission:^(
                            BOOL granted, BOOL promptShown, NSError* error) {
    if (!error && !promptShown && !granted) {
      // This callback can be executed on a background thread, make sure
      // the UI is displayed on the main thread.
      dispatch_async(dispatch_get_main_queue(), ^{
        [weakSelf.presenter presentPushNotificationPermissionAlert];
      });
    }
  }];
}

- (void)emailNotificationsSwitchToggled:(UISwitch*)sender {
  _prefService->SetBoolean(commerce::kPriceEmailNotificationsEnabled,
                           sender.on);
  [self booleanDidChange:_emailNotificationsEnabled];
}

@end

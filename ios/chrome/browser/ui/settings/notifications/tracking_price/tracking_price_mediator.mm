// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/notifications/tracking_price/tracking_price_mediator.h"

#import "base/apple/foundation_util.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/commerce/core/pref_names.h"
#import "components/commerce/core/price_tracking_utils.h"
#import "components/commerce/core/shopping_service.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/push_notification/model/push_notification_account_context_manager.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/push_notification/model/push_notification_profile_service.h"
#import "ios/chrome/browser/push_notification/model/push_notification_profile_service_factory.h"
#import "ios/chrome/browser/push_notification/model/push_notification_service.h"
#import "ios/chrome/browser/push_notification/model/push_notification_settings_util.h"
#import "ios/chrome/browser/push_notification/model/push_notification_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/ui/settings/notifications/tracking_price/tracking_price_alert_presenter.h"
#import "ios/chrome/browser/ui/settings/notifications/tracking_price/tracking_price_constants.h"
#import "ios/chrome/browser/ui/settings/notifications/tracking_price/tracking_price_consumer.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

// List of items.
typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeMobileNotifications = kItemTypeEnumZero,
  ItemTypeTrackPriceHeader,
  ItemTypeEmailNotifications,
};

@interface TrackingPriceMediator ()

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
  }

  return self;
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
            PushNotificationClientId::kCommerce,
            base::SysNSStringToUTF8(_identity.gaiaID));
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

#pragma mark - TrackingPriceViewControllerDelegate

- (void)toggleSwitchItem:(TableViewItem*)item withValue:(BOOL)value {
  ItemType type = static_cast<ItemType>(item.type);
  switch (type) {
    case ItemTypeMobileNotifications: {
      [self setPreferenceFor:PushNotificationClientId::kCommerce to:value];
      self.mobileNotificationItem.on = push_notification_settings::
          GetMobileNotificationPermissionStatusForClient(
              PushNotificationClientId::kCommerce,
              base::SysNSStringToUTF8(_identity.gaiaID));
      if (!value) {
        break;
      }

      __weak TrackingPriceMediator* weakSelf = self;
      [PushNotificationUtil
          requestPushNotificationPermission:^(BOOL granted, BOOL promptShown,
                                              NSError* error) {
            if (!error && !promptShown && !granted) {
              // This callback can be executed on a background thread, make sure
              // the UI is displayed on the main thread.
              dispatch_async(dispatch_get_main_queue(), ^{
                [weakSelf.presenter presentPushNotificationPermissionAlert];
              });
            }
          }];
      break;
    }
    case ItemTypeEmailNotifications: {
      _prefService->SetBoolean(commerce::kPriceEmailNotificationsEnabled,
                               value);
      self.emailNotificationItem.on = value;
      break;
    }
    default:
      // Not a switch.
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

#pragma mark - Private

// Updates the current user's permission preference for the given `client_id`.
- (void)setPreferenceFor:(PushNotificationClientId)clientID to:(BOOL)enabled {
  PushNotificationService* service =
      GetApplicationContext()->GetPushNotificationService();
  service->SetPreference(_identity.gaiaID, clientID, enabled);
}

@end

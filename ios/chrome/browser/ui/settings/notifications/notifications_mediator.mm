// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/notifications/notifications_mediator.h"

#import "base/apple/foundation_util.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/push_notification/model/push_notification_account_context_manager.h"
#import "ios/chrome/browser/push_notification/model/push_notification_browser_state_service.h"
#import "ios/chrome/browser/push_notification/model/push_notification_browser_state_service_factory.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/push_notification/model/push_notification_service.h"
#import "ios/chrome/browser/push_notification/model/push_notification_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/ui/settings/notifications/notifications_alert_presenter.h"
#import "ios/chrome/browser/ui/settings/notifications/notifications_constants.h"
#import "ios/chrome/browser/ui/settings/notifications/notifications_consumer.h"
#import "ios/chrome/browser/ui/settings/notifications/notifications_navigation_commands.h"
#import "ios/chrome/browser/ui/settings/notifications/notifications_settings_observer.h"
#import "ios/chrome/browser/ui/settings/notifications/notifications_settings_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

// List of items.
typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeTrackingPrice = kItemTypeEnumZero,
  ItemTypeContentNotifications,
  ItemTypeContentNotificationsFooter,
};

@interface NotificationsMediator ()

// All the items for the price notifications section.
@property(nonatomic, strong, readonly) TableViewItem* priceTrackingItem;
// Items for the Content Notifications settings.
@property(nonatomic, strong, readonly)
    TableViewSwitchItem* contentNotificationsItem;
// Item for the Content Notifications footer.
@property(nonatomic, strong)
    TableViewLinkHeaderFooterItem* contentNotificationsFooterItem;
// Pref Service object.
@property(nonatomic, assign) PrefService* prefService;

@end

@implementation NotificationsMediator {
  // Identity object that contains the user's account details.
  std::string _gaiaID;
}

@synthesize priceTrackingItem = _priceTrackingItem;
@synthesize contentNotificationsItem = _contentNotificationsItem;

- (instancetype)initWithPrefService:(PrefService*)prefs
                             gaiaID:(const std::string&)gaiaID {
  self = [super init];
  if (self) {
    DCHECK(prefs);
    _prefService = prefs;
    _gaiaID = gaiaID;
  }

  return self;
}

#pragma mark - Properties

- (TableViewItem*)priceTrackingItem {
  if (!_priceTrackingItem) {
    _priceTrackingItem = [self
             detailItemWithType:ItemTypeTrackingPrice
                           text:
                               l10n_util::GetNSString(
                                   IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACKING_TITLE)
                     detailText:nil
                         symbol:kDownTrendSymbol
          symbolBackgroundColor:[UIColor colorNamed:kPink500Color]
        accessibilityIdentifier:kSettingsNotificationsPriceTrackingCellId];
    [self updateDetailTextForItem:_priceTrackingItem
                     withClientID:PushNotificationClientId::kCommerce];
  }
  return _priceTrackingItem;
}

- (TableViewSwitchItem*)contentNotificationsItem {
  if (!_contentNotificationsItem) {
    _contentNotificationsItem =
        [self switchItemWithType:ItemTypeContentNotifications
                               text:@"Personalized Content"
                             symbol:kNewspaperSFSymbol
              symbolBackgroundColor:[UIColor colorNamed:kPink500Color]
            accessibilityIdentifier:kSettingsNotificationsContentCellId];
    _contentNotificationsItem.on =
        notifications_settings::GetMobileNotificationPermissionStatusForClient(
            PushNotificationClientId::kContent, _gaiaID);
  }
  return _contentNotificationsItem;
}

- (TableViewLinkHeaderFooterItem*)contentNotificationsFooterItem {
  if (!_contentNotificationsFooterItem) {
    _contentNotificationsFooterItem = [[TableViewLinkHeaderFooterItem alloc]
        initWithType:ItemTypeContentNotificationsFooter];
    _contentNotificationsFooterItem.text =
        @"Get notified with personalized news updates and more.";
  }

  return _contentNotificationsFooterItem;
}

- (void)setConsumer:(id<NotificationsConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }
  _consumer = consumer;
  [_consumer setPriceTrackingItem:self.priceTrackingItem];
  [_consumer setContentNotificationsItem:self.contentNotificationsItem];
  [_consumer
      setContentNotificationsFooterItem:self.contentNotificationsFooterItem];
}

#pragma mark - Private methods

// Creates item with details and icon image.
- (TableViewDetailIconItem*)detailItemWithType:(NSInteger)type
                                          text:(NSString*)text
                                    detailText:(NSString*)detailText
                                        symbol:(NSString*)symbol
                         symbolBackgroundColor:(UIColor*)backgroundColor
                       accessibilityIdentifier:
                           (NSString*)accessibilityIdentifier {
  TableViewDetailIconItem* detailItem =
      [[TableViewDetailIconItem alloc] initWithType:type];
  detailItem.text = text;
  detailItem.detailText = detailText;
  detailItem.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  detailItem.accessibilityTraits |= UIAccessibilityTraitButton;
  detailItem.accessibilityIdentifier = accessibilityIdentifier;
  detailItem.iconImage = CustomSettingsRootSymbol(symbol);
  detailItem.iconTintColor = UIColor.whiteColor;
  detailItem.iconCornerRadius = kColorfulBackgroundSymbolCornerRadius;
  detailItem.iconBackgroundColor = backgroundColor;

  return detailItem;
}

- (TableViewSwitchItem*)switchItemWithType:(NSInteger)type
                                      text:(NSString*)text
                                    symbol:(NSString*)symbol
                     symbolBackgroundColor:(UIColor*)backgroundColor
                   accessibilityIdentifier:(NSString*)accessibilityIdentifier {
  TableViewSwitchItem* switchItem =
      [[TableViewSwitchItem alloc] initWithType:type];
  switchItem.text = text;
  switchItem.accessibilityIdentifier = accessibilityIdentifier;
  switchItem.iconImage = DefaultSettingsRootSymbol(symbol);
  switchItem.iconTintColor = UIColor.whiteColor;
  switchItem.iconCornerRadius = kColorfulBackgroundSymbolCornerRadius;
  switchItem.iconBackgroundColor = backgroundColor;

  return switchItem;
}

// Updates the detail text for the TableViewItem located in
// PriceNotificationsTableViewController on the previous screen to read either
// 'On/Off' to match the change to the client's push notification permission
// state.
- (void)updateDetailTextForItem:(TableViewItem*)item
                   withClientID:(PushNotificationClientId)clientID {
  DCHECK(item);
  TableViewDetailIconItem* iconItem =
      base::apple::ObjCCastStrict<TableViewDetailIconItem>(item);
  notifications_settings::ClientPermissionState permissionState =
      notifications_settings::GetClientPermissionState(clientID, _gaiaID,
                                                       _prefService);
  NSString* detailText = nil;
  if (permissionState ==
      notifications_settings::ClientPermissionState::ENABLED) {
    detailText = l10n_util::GetNSString(IDS_IOS_SETTING_ON);
  } else if (permissionState ==
             notifications_settings::ClientPermissionState::DISABLED) {
    detailText = l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
  }

  iconItem.detailText = detailText;
  [self.consumer reconfigureCellsForItems:@[ iconItem ]];
}

#pragma mark - NotificationsViewControllerDelegate

- (void)didToggleSwitchItem:(TableViewSwitchItem*)item withValue:(BOOL)value {
  ItemType type = static_cast<ItemType>(item.type);
  switch (type) {
    case ItemTypeContentNotifications: {
      [self setPreferenceFor:PushNotificationClientId::kContent to:value];
      self.contentNotificationsItem.on = notifications_settings::
          GetMobileNotificationPermissionStatusForClient(
              PushNotificationClientId::kContent, _gaiaID);
      if (!value) {
        break;
      }

      __weak NotificationsMediator* weakSelf = self;
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
    default:
      // Not a switch.
      NOTREACHED();
      break;
  }
}

- (void)didSelectItem:(TableViewItem*)item {
  ItemType type = static_cast<ItemType>(item.type);
  switch (type) {
    case ItemTypeTrackingPrice:
      [self.handler showTrackingPrice];
      break;
    case ItemTypeContentNotifications:
      break;
    default:
      NOTREACHED();
      break;
  }
}

#pragma mark - NotificationsSettingsObserverDelegate

- (void)notificationsSettingsDidChangeForClient:
    (PushNotificationClientId)clientID {
  switch (clientID) {
    case PushNotificationClientId::kCommerce: {
      [self updateDetailTextForItem:_priceTrackingItem withClientID:clientID];
      break;
    }
    // TODO(b/307593022): Move Notification popup logic here when the pref is
    // ready.
    case PushNotificationClientId::kContent: {
      break;
    }
  }
}

// Updates the current user's permission preference for the given `client_id`.
- (void)setPreferenceFor:(PushNotificationClientId)clientID to:(BOOL)enabled {
  PushNotificationService* service =
      GetApplicationContext()->GetPushNotificationService();
  service->SetPreference(base::SysUTF8ToNSString(_gaiaID), clientID, enabled);
}

@end

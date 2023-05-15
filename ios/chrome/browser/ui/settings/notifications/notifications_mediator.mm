// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/notifications/notifications_mediator.h"

#import "base/mac/foundation_util.h"
#import "base/notreached.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/push_notification/push_notification_client_id.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/ui/settings/notifications/notifications_constants.h"
#import "ios/chrome/browser/ui/settings/notifications/notifications_consumer.h"
#import "ios/chrome/browser/ui/settings/notifications/notifications_navigation_commands.h"
#import "ios/chrome/browser/ui/settings/notifications/notifications_settings_observer.h"
#import "ios/chrome/browser/ui/settings/notifications/notifications_settings_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// List of items.
typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeTrackingPrice = kItemTypeEnumZero,
};

@interface NotificationsMediator ()

// All the items for the price notifications section.
@property(nonatomic, strong, readonly) TableViewItem* priceTrackingItem;

// Pref Service object.
@property(nonatomic, assign) PrefService* prefService;

@end

@implementation NotificationsMediator {
  // Identity object that contains the user's account details.
  std::string _gaiaID;
}

@synthesize priceTrackingItem = _priceTrackingItem;

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

- (void)setConsumer:(id<NotificationsConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }
  _consumer = consumer;
  [_consumer setPriceTrackingItem:self.priceTrackingItem];
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

// Updates the detail text for the TableViewItem located in
// PriceNotificationsTableViewController on the previous screen to read either
// 'On/Off' to match the change to the client's push notification permission
// state.
- (void)updateDetailTextForItem:(TableViewItem*)item
                   withClientID:(PushNotificationClientId)clientID {
  DCHECK(item);
  TableViewDetailIconItem* iconItem =
      base::mac::ObjCCastStrict<TableViewDetailIconItem>(item);
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

#pragma mark - PriceNotificationsViewControllerDelegate

- (void)didSelectItem:(TableViewItem*)item {
  ItemType type = static_cast<ItemType>(item.type);
  switch (type) {
    case ItemTypeTrackingPrice:
      [self.handler showTrackingPrice];
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
  }
}

@end

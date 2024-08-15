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
#import "ios/chrome/browser/push_notification/model/push_notification_settings_util.h"
#import "ios/chrome/browser/push_notification/model/push_notification_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_multi_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/ui/push_notification/notifications_alert_presenter.h"
#import "ios/chrome/browser/ui/settings/notifications/notifications_constants.h"
#import "ios/chrome/browser/ui/settings/notifications/notifications_consumer.h"
#import "ios/chrome/browser/ui/settings/notifications/notifications_item_identifier.h"
#import "ios/chrome/browser/ui/settings/notifications/notifications_navigation_commands.h"
#import "ios/chrome/browser/ui/settings/notifications/notifications_settings_observer.h"
#import "ios/chrome/browser/ui/settings/notifications/tips_notifications_alert_presenter.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@interface NotificationsMediator ()

// All the items for the price notifications section.
@property(nonatomic, strong, readonly) TableViewItem* priceTrackingItem;
// Items for the Content Notifications settings.
@property(nonatomic, strong, readonly) TableViewItem* contentNotificationsItem;
// Items for the Tips Notifications settings.
@property(nonatomic, strong, readonly)
    TableViewSwitchItem* tipsNotificationsItem;
// Item for the Tips Notifications footer.
@property(nonatomic, strong)
    TableViewLinkHeaderFooterItem* tipsNotificationsFooterItem;
// Pref Service object.
@property(nonatomic, assign) PrefService* prefService;

@end

@implementation NotificationsMediator {
  // Identity object that contains the user's account details.
  std::string _gaiaID;
}

@synthesize priceTrackingItem = _priceTrackingItem;
@synthesize contentNotificationsItem = _contentNotificationsItem;
@synthesize tipsNotificationsItem = _tipsNotificationsItem;

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

#pragma mark - Public

- (void)deniedPermissionsForClientIds:
    (std::vector<PushNotificationClientId>)clientIds {
  for (PushNotificationClientId clientID : clientIds) {
    [self switchItemForClientId:clientID].on = push_notification_settings::
        GetMobileNotificationPermissionStatusForClient(clientID, _gaiaID);
  }
  [self.consumer reloadData];
}

#pragma mark - Properties

- (TableViewItem*)priceTrackingItem {
  if (!_priceTrackingItem) {
    if (IsIOSTipsNotificationsEnabled()) {
      _priceTrackingItem = [self
               detailItemWithType:NotificationsItemIdentifier::
                                      ItemIdentifierPriceTracking
                             text:
                                 l10n_util::GetNSString(
                                     IDS_IOS_NOTIFICATIONS_OPT_IN_PRICE_TRACKING_TOGGLE_TITLE)
               trailingDetailText:nil
                leadingDetailText:
                    l10n_util::GetNSString(
                        IDS_IOS_NOTIFICATIONS_OPT_IN_PRICE_TRACKING_TOGGLE_MESSAGE)
          accessibilityIdentifier:kSettingsNotificationsPriceTrackingCellId];
    } else {
      _priceTrackingItem = [self
               detailItemWithType:NotificationsItemIdentifier::
                                      ItemIdentifierPriceTracking
                             text:
                                 l10n_util::GetNSString(
                                     IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACKING_TITLE)
                       detailText:nil
                           symbol:CustomSettingsRootSymbol(kDownTrendSymbol)
                       symbolTint:UIColor.whiteColor
            symbolBackgroundColor:[UIColor colorNamed:kPink500Color]
          accessibilityIdentifier:kSettingsNotificationsPriceTrackingCellId];
    }
    [self updateDetailTextForItem:_priceTrackingItem
                    withClientIDs:{PushNotificationClientId::kCommerce}];
  }
  return _priceTrackingItem;
}

- (TableViewItem*)contentNotificationsItem {
  if (!_contentNotificationsItem) {
    if (IsIOSTipsNotificationsEnabled()) {
      _contentNotificationsItem = [self
               detailItemWithType:NotificationsItemIdentifier::
                                      ItemIdentifierContent
                             text:
                                 l10n_util::GetNSString(
                                     IDS_IOS_CONTENT_NOTIFICATIONS_CONTENT_SETTINGS_TOGGLE_TITLE)
               trailingDetailText:nil
                leadingDetailText:
                    l10n_util::GetNSString(
                        IDS_IOS_CONTENT_NOTIFICATIONS_CONTENT_SETTINGS_FOOTER_TEXT)
          accessibilityIdentifier:kSettingsNotificationsContentCellId];
    } else {
      _contentNotificationsItem = [self
               detailItemWithType:NotificationsItemIdentifier::
                                      ItemIdentifierContent
                             text:
                                 l10n_util::GetNSString(
                                     IDS_IOS_CONTENT_NOTIFICATIONS_CONTENT_SETTINGS_TOGGLE_TITLE)
                       detailText:nil
                           symbol:DefaultSettingsRootSymbol(kNewspaperSFSymbol)
                       symbolTint:UIColor.whiteColor
            symbolBackgroundColor:[UIColor colorNamed:kPink500Color]
          accessibilityIdentifier:kSettingsNotificationsContentCellId];
    }
    [self updateDetailTextForItem:_contentNotificationsItem
                    withClientIDs:{PushNotificationClientId::kContent,
                                   PushNotificationClientId::kSports}];
  }
  return _contentNotificationsItem;
}

- (TableViewSwitchItem*)tipsNotificationsItem {
  if (!_tipsNotificationsItem) {
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
    UIImage* image = MakeSymbolMulticolor(
        CustomSettingsRootSymbol(kMulticolorChromeballSymbol));
#else
    UIImage* image = CustomSettingsRootSymbol(kChromeProductSymbol);
#endif  // BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
    _tipsNotificationsItem = [self
             switchItemWithType:NotificationsItemIdentifier::ItemIdentifierTips
                           text:l10n_util::GetNSString(
                                    IDS_IOS_SET_UP_LIST_TIPS_TITLE)
                     detailText:l10n_util::GetNSString(
                                    IDS_IOS_TIPS_NOTIFICATION_SETTINGS_FOOTER)
                         symbol:image
                     symbolTint:nil
          symbolBackgroundColor:nil
              symbolBorderWidth:1
        accessibilityIdentifier:kSettingsNotificationsContentCellId];
    _tipsNotificationsItem.on = push_notification_settings::
        GetMobileNotificationPermissionStatusForClient(
            PushNotificationClientId::kTips, _gaiaID);
  }
  return _tipsNotificationsItem;
}

- (TableViewLinkHeaderFooterItem*)tipsNotificationsFooterItem {
  if (!_tipsNotificationsFooterItem) {
    _tipsNotificationsFooterItem = [[TableViewLinkHeaderFooterItem alloc]
        initWithType:NotificationsItemIdentifier::
                         ItemIdentifierTipsNotificationsFooter];
    _tipsNotificationsFooterItem.text =
        l10n_util::GetNSString(IDS_IOS_TIPS_NOTIFICATION_SETTINGS_FOOTER);
  }

  return _tipsNotificationsFooterItem;
}

- (void)setConsumer:(id<NotificationsConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }
  _consumer = consumer;
  [_consumer setPriceTrackingItem:self.priceTrackingItem];
  [_consumer setContentNotificationsItem:self.contentNotificationsItem];
  if (IsIOSTipsNotificationsEnabled()) {
    [_consumer setTipsNotificationsItem:self.tipsNotificationsItem];
    [_consumer setTipsNotificationsFooterItem:self.tipsNotificationsFooterItem];
  }
}

#pragma mark - Private methods

// Creates item with two text labels and icon image.
- (TableViewDetailIconItem*)detailItemWithType:(NSInteger)type
                                          text:(NSString*)text
                                    detailText:(NSString*)detailText
                                        symbol:(UIImage*)symbol
                                    symbolTint:(UIColor*)tint
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
  detailItem.iconImage = symbol;
  detailItem.iconTintColor = tint;
  detailItem.iconCornerRadius = kColorfulBackgroundSymbolCornerRadius;
  detailItem.iconBackgroundColor = backgroundColor;

  return detailItem;
}

- (TableViewSwitchItem*)switchItemWithType:(NSInteger)type
                                      text:(NSString*)text
                                detailText:(NSString*)detailText
                                    symbol:(UIImage*)symbol
                                symbolTint:(UIColor*)tint
                     symbolBackgroundColor:(UIColor*)backgroundColor
                         symbolBorderWidth:(CGFloat)borderWidth
                   accessibilityIdentifier:(NSString*)accessibilityIdentifier {
  TableViewSwitchItem* switchItem =
      [[TableViewSwitchItem alloc] initWithType:type];
  switchItem.text = text;
  switchItem.accessibilityIdentifier = accessibilityIdentifier;
  if (IsIOSTipsNotificationsEnabled()) {
    switchItem.detailText = detailText;
  } else {
    switchItem.iconImage = symbol;
    switchItem.iconTintColor = tint;
    switchItem.iconCornerRadius = kColorfulBackgroundSymbolCornerRadius;
    switchItem.iconBackgroundColor = backgroundColor;
    switchItem.iconBorderWidth = borderWidth;
  }

  return switchItem;
}

// Creates item with three text labels.
- (TableViewMultiDetailTextItem*)
         detailItemWithType:(NSInteger)type
                       text:(NSString*)text
         trailingDetailText:(NSString*)trailingDetailText
          leadingDetailText:(NSString*)leadingDetailText
    accessibilityIdentifier:(NSString*)accessibilityIdentifier {
  TableViewMultiDetailTextItem* detailItem =
      [[TableViewMultiDetailTextItem alloc] initWithType:type];
  detailItem.text = text;
  detailItem.trailingDetailText = trailingDetailText;
  detailItem.leadingDetailText = leadingDetailText;
  detailItem.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  detailItem.accessibilityTraits |= UIAccessibilityTraitButton;
  detailItem.accessibilityIdentifier = accessibilityIdentifier;

  return detailItem;
}

// Updates the detail text for the TableViewItem located in
// PriceNotificationsTableViewController on the previous screen to read either
// 'On/Off' to match the change to the client's push notification permission
// state.
- (void)updateDetailTextForItem:(TableViewItem*)item
                  withClientIDs:
                      (std::vector<PushNotificationClientId>)clientIDs {
  DCHECK(item);
  push_notification_settings::ClientPermissionState permissionState;
  permissionState =
      push_notification_settings::GetClientPermissionStateForMultipleClients(
          clientIDs, _gaiaID, _prefService);
  NSString* detailText = nil;
  if (permissionState ==
      push_notification_settings::ClientPermissionState::ENABLED) {
    detailText = l10n_util::GetNSString(IDS_IOS_SETTING_ON);
  } else if (permissionState ==
             push_notification_settings::ClientPermissionState::DISABLED) {
    detailText = l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
  }

  if (IsIOSTipsNotificationsEnabled()) {
    TableViewMultiDetailTextItem* detailItem =
        base::apple::ObjCCastStrict<TableViewMultiDetailTextItem>(item);
    detailItem.trailingDetailText = detailText;
    [self.consumer reconfigureCellsForItems:@[ detailItem ]];
  } else {
    TableViewDetailIconItem* iconItem =
        base::apple::ObjCCastStrict<TableViewDetailIconItem>(item);
    iconItem.detailText = detailText;
    [self.consumer reconfigureCellsForItems:@[ iconItem ]];
  }
}

#pragma mark - NotificationsViewControllerDelegate

- (void)didToggleSwitchItem:(TableViewSwitchItem*)item withValue:(BOOL)value {
  NotificationsItemIdentifier itemIdentifier =
      static_cast<NotificationsItemIdentifier>(item.type);
  switch (itemIdentifier) {
    case ItemIdentifierTips: {
      if (value) {
        [self.presenter presentTipsNotificationPermissionAlert];
      } else {
        [self disablePreferenceFor:PushNotificationClientId::kTips];
        self.tipsNotificationsItem.on = push_notification_settings::
            GetMobileNotificationPermissionStatusForClient(
                PushNotificationClientId::kTips, _gaiaID);
      }
      break;
    }
    default:
      // Not a switch.
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

- (void)didSelectItem:(TableViewItem*)item {
  NotificationsItemIdentifier type =
      static_cast<NotificationsItemIdentifier>(item.type);
  switch (type) {
    case ItemIdentifierPriceTracking:
      [self.handler showTrackingPrice];
      break;
    case ItemIdentifierContent:
      [self.handler showContent];
      break;
    case ItemIdentifierTips:
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

#pragma mark - NotificationsSettingsObserverDelegate

- (void)notificationsSettingsDidChangeForClient:
    (PushNotificationClientId)clientID {
  switch (clientID) {
    case PushNotificationClientId::kCommerce: {
      [self updateDetailTextForItem:_priceTrackingItem
                      withClientIDs:{clientID}];
      break;
    }
    case PushNotificationClientId::kContent:
    case PushNotificationClientId::kSports: {
      [self updateDetailTextForItem:_contentNotificationsItem
                      withClientIDs:{PushNotificationClientId::kSports,
                                     PushNotificationClientId::kContent}];
      break;
    }
    case PushNotificationClientId::kTips: {
      self.tipsNotificationsItem.on = push_notification_settings::
          GetMobileNotificationPermissionStatusForClient(
              PushNotificationClientId::kTips, _gaiaID);
      [self.consumer reconfigureCellsForItems:@[ self.tipsNotificationsItem ]];
      break;
    }
    case PushNotificationClientId::kSafetyCheck:
      // TODO(crbug.com/347975024): Integrate Safety Check Notifications with
      // notifications settings UI.
      break;
  }
}

#pragma mark - private

// Updates the current user's permission preference for the given `client_id`.
- (void)disablePreferenceFor:(PushNotificationClientId)clientID {
  PushNotificationService* service =
      GetApplicationContext()->GetPushNotificationService();
  service->SetPreference(base::SysUTF8ToNSString(_gaiaID), clientID, false);
}

// Returns the TableViewSwitchItem for the given `clientId`.
- (TableViewSwitchItem*)switchItemForClientId:
    (PushNotificationClientId)clientId {
  switch (clientId) {
    case PushNotificationClientId::kTips:
      return _tipsNotificationsItem;
    case PushNotificationClientId::kCommerce:
    case PushNotificationClientId::kSafetyCheck:
    case PushNotificationClientId::kContent:
    case PushNotificationClientId::kSports:
      // Not a switch.
      NOTREACHED();
  }
}

@end

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/notifications/content_notifications/content_notifications_mediator.h"

#import "base/apple/foundation_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/content_notification/model/content_notification_nau_configuration.h"
#import "ios/chrome/browser/content_notification/model/content_notification_service.h"
#import "ios/chrome/browser/content_notification/model/content_notification_settings_action.h"
#import "ios/chrome/browser/push_notification/model/constants.h"
#import "ios/chrome/browser/push_notification/model/push_notification_account_context_manager.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/push_notification/model/push_notification_profile_service.h"
#import "ios/chrome/browser/push_notification/model/push_notification_profile_service_factory.h"
#import "ios/chrome/browser/push_notification/model/push_notification_service.h"
#import "ios/chrome/browser/push_notification/model/push_notification_settings_util.h"
#import "ios/chrome/browser/push_notification/model/push_notification_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/ui/push_notification/notifications_alert_presenter.h"
#import "ios/chrome/browser/ui/settings/notifications/content_notifications/content_notifications_constants.h"
#import "ios/chrome/browser/ui/settings/notifications/content_notifications/content_notifications_consumer.h"
#import "ios/chrome/browser/ui/settings/notifications/notifications_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

// List of items.
typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeContentNotifications = kItemTypeEnumZero,
  ItemTypeSportsNotifications,
  ItemTypeContentFooter,
};

@interface ContentNotificationsMediator ()

// Items for the Content Notifications settings.
@property(nonatomic, strong, readonly)
    TableViewSwitchItem* contentNotificationsItem;
// Items for the Sports Notifications settings.
@property(nonatomic, strong, readonly)
    TableViewSwitchItem* sportsNotificationsItem;
// Header item.
@property(nonatomic, strong)
    TableViewLinkHeaderFooterItem* contentNotificationsFooterItem;

// Pref service to retrieve preference values.
@property(nonatomic, assign) PrefService* prefService;

@end

@implementation ContentNotificationsMediator {
  // Identity object that contains the user's account details.
  std::string _gaiaID;
}

@synthesize contentNotificationsItem = _contentNotificationsItem;
@synthesize contentNotificationsFooterItem = _contentNotificationsFooterItem;
@synthesize sportsNotificationsItem = _sportsNotificationsItem;

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

- (TableViewSwitchItem*)contentNotificationsItem {
  if (!_contentNotificationsItem) {
    _contentNotificationsItem = [self
             switchItemWithType:ItemTypeContentNotifications
                           text:
                               l10n_util::GetNSString(
                                   IDS_IOS_CONTENT_NOTIFICATIONS_PERSONALIZED_NEWS_SETTINGS_TOGGLE_TITLE)
                         symbol:DefaultSettingsRootSymbol(kNewspaperSFSymbol)
                     symbolTint:UIColor.whiteColor
          symbolBackgroundColor:[UIColor colorNamed:kPink500Color]
              symbolBorderWidth:0
        accessibilityIdentifier:kContentNotificationsCellId];
    _contentNotificationsItem.on = push_notification_settings::
        GetMobileNotificationPermissionStatusForClient(
            PushNotificationClientId::kContent, _gaiaID);
  }
  return _contentNotificationsItem;
}

- (TableViewSwitchItem*)sportsNotificationsItem {
  if (!_sportsNotificationsItem) {
    _sportsNotificationsItem = [self
             switchItemWithType:ItemTypeSportsNotifications
                           text:
                               l10n_util::GetNSString(
                                   IDS_IOS_CONTENT_NOTIFICATIONS_SPORTS_SETTINGS_TOGGLE_TITLE)
                         symbol:DefaultSettingsRootSymbol(kMedalSymbol)
                     symbolTint:UIColor.whiteColor
          symbolBackgroundColor:[UIColor colorNamed:kPink500Color]
              symbolBorderWidth:0
        accessibilityIdentifier:kSportsNotificationsCellId];
    _sportsNotificationsItem.on = push_notification_settings::
        GetMobileNotificationPermissionStatusForClient(
            PushNotificationClientId::kSports, _gaiaID);
  }
  return _sportsNotificationsItem;
}

- (TableViewLinkHeaderFooterItem*)contentNotificationsFooterItem {
  if (!_contentNotificationsFooterItem) {
    _contentNotificationsFooterItem = [[TableViewLinkHeaderFooterItem alloc]
        initWithType:ItemTypeContentFooter];
    _contentNotificationsFooterItem.text = l10n_util::GetNSString(
        IDS_IOS_CONTENT_NOTIFICATIONS_CONTENT_SETTINGS_FOOTER_TEXT);
  }

  return _contentNotificationsFooterItem;
}

- (void)setConsumer:(id<ContentNotificationsConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }
  _consumer = consumer;
  [_consumer setContentNotificationsItem:self.contentNotificationsItem];
  [_consumer setSportsNotificationsItem:self.sportsNotificationsItem];
  [_consumer
      setContentNotificationsFooterItem:self.contentNotificationsFooterItem];
}

#pragma mark - ContentNotificationsViewControllerDelegate

- (void)didToggleSwitchItem:(TableViewItem*)item withValue:(BOOL)value {
  ItemType type = static_cast<ItemType>(item.type);
  switch (type) {
    case ItemTypeContentNotifications: {
      if (value) {
        [self.presenter presentPushNotificationPermissionAlertWithClientIds:
                            {PushNotificationClientId::kContent}];
        [self recordSettingsActionHistogramForAction:
                  ContentNotificationSettingsToggleAction::kEnabledContent];
      } else {
        [self disablePreferenceFor:PushNotificationClientId::kContent];
        self.contentNotificationsItem.on = push_notification_settings::
            GetMobileNotificationPermissionStatusForClient(
                PushNotificationClientId::kContent, _gaiaID);
        [self recordSettingsActionHistogramForAction:
                  ContentNotificationSettingsToggleAction::kDisabledContent];
      }
      [self sendNAUForPreferenceChangeWithClientID:PushNotificationClientId::
                                                       kContent
                                             value:value];
      break;
    }
    case ItemTypeSportsNotifications: {
      if (value) {
        [self.presenter presentPushNotificationPermissionAlertWithClientIds:
                            {PushNotificationClientId::kSports}];
        [self recordSettingsActionHistogramForAction:
                  ContentNotificationSettingsToggleAction::kEnabledSports];
      } else {
        [self disablePreferenceFor:PushNotificationClientId::kSports];
        self.sportsNotificationsItem.on = push_notification_settings::
            GetMobileNotificationPermissionStatusForClient(
                PushNotificationClientId::kSports, _gaiaID);
        [self recordSettingsActionHistogramForAction:
                  ContentNotificationSettingsToggleAction::kDisabledSports];
      }
      [self sendNAUForPreferenceChangeWithClientID:PushNotificationClientId::
                                                       kSports
                                             value:value];
      break;
    }
    default:
      // Not a switch.
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

#pragma mark - Private

- (TableViewSwitchItem*)switchItemWithType:(NSInteger)type
                                      text:(NSString*)text
                                    symbol:(UIImage*)symbol
                                symbolTint:(UIColor*)tint
                     symbolBackgroundColor:(UIColor*)backgroundColor
                         symbolBorderWidth:(CGFloat)borderWidth
                   accessibilityIdentifier:(NSString*)accessibilityIdentifier {
  TableViewSwitchItem* switchItem =
      [[TableViewSwitchItem alloc] initWithType:type];
  switchItem.text = text;
  switchItem.accessibilityIdentifier = accessibilityIdentifier;
  switchItem.iconImage = symbol;
  switchItem.iconTintColor = tint;
  switchItem.iconCornerRadius = kColorfulBackgroundSymbolCornerRadius;
  switchItem.iconBackgroundColor = backgroundColor;
  switchItem.iconBorderWidth = borderWidth;

  return switchItem;
}

// Updates the current user's permission preference for the given `client_id`.
- (void)disablePreferenceFor:(PushNotificationClientId)clientID {
  PushNotificationService* service =
      GetApplicationContext()->GetPushNotificationService();
  service->SetPreference(base::SysUTF8ToNSString(_gaiaID), clientID, false);
}

// Sends an NAU when any of the settings preferences have been updated.
- (void)sendNAUForPreferenceChangeWithClientID:
            (PushNotificationClientId)clientID
                                         value:(BOOL)value {
  ContentNotificationSettingsAction* settingsAction =
      [[ContentNotificationSettingsAction alloc] init];
  settingsAction.toggleStatus = value;
  switch (clientID) {
    case PushNotificationClientId::kContent:
      settingsAction.toggleChanged = SettingsToggleTypeContent;
      break;
    case PushNotificationClientId::kSports:
      settingsAction.toggleChanged = SettingsToggleTypeSports;
      break;
    case PushNotificationClientId::kCommerce:
    case PushNotificationClientId::kTips:
    case PushNotificationClientId::kSafetyCheck:
    case PushNotificationClientId::kSendTab:
      // This should never be reached.
      DCHECK(FALSE);
      break;
  }
  [PushNotificationUtil
      getPermissionSettings:^(UNNotificationSettings* settings) {
        settingsAction.currentAuthorizationStatus =
            settings.authorizationStatus;
        ContentNotificationNAUConfiguration* config =
            [[ContentNotificationNAUConfiguration alloc] init];
        config.settingsAction = settingsAction;
        self.contentNotificationService->SendNAUForConfiguration(config);
      }];
}

// Returns the TableViewSwitchItem for the given `clientId`.
- (TableViewSwitchItem*)switchItemForClientId:
    (PushNotificationClientId)clientId {
  switch (clientId) {
    case PushNotificationClientId::kContent:
      return _contentNotificationsItem;
    case PushNotificationClientId::kSports:
      return _sportsNotificationsItem;
    case PushNotificationClientId::kTips:
    case PushNotificationClientId::kSendTab:
    case PushNotificationClientId::kSafetyCheck:
    case PushNotificationClientId::kCommerce:
      // Not a switch.
      NOTREACHED();
  }
}

- (void)recordSettingsActionHistogramForAction:
    (ContentNotificationSettingsToggleAction)action {
  base::UmaHistogramEnumeration("ContentNotifications.Settings.Action", action);
}

@end

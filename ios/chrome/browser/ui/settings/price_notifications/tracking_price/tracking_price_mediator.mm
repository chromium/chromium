// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/price_notifications/tracking_price/tracking_price_mediator.h"

#import "base/mac/foundation_util.h"
#import "base/notreached.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/push_notification/push_notification_account_context_manager.h"
#import "ios/chrome/browser/push_notification/push_notification_browser_state_service.h"
#import "ios/chrome/browser/push_notification/push_notification_browser_state_service_factory.h"
#import "ios/chrome/browser/push_notification/push_notification_client_id.h"
#import "ios/chrome/browser/push_notification/push_notification_service.h"
#import "ios/chrome/browser/push_notification/push_notification_util.h"
#import "ios/chrome/browser/ui/list_model/list_model.h"
#import "ios/chrome/browser/ui/settings/price_notifications/tracking_price/tracking_price_alert_presenter.h"
#import "ios/chrome/browser/ui/settings/price_notifications/tracking_price/tracking_price_constants.h"
#import "ios/chrome/browser/ui/settings/price_notifications/tracking_price/tracking_price_consumer.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// List of items.
typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeMobileNotifications = kItemTypeEnumZero,
  ItemTypeTrackPriceHeader,
};

@interface TrackingPriceMediator () {
  base::WeakPtr<ChromeBrowserState> _browserState;
}

// Header item.
@property(nonatomic, strong)
    TableViewLinkHeaderFooterItem* trackPriceHeaderItem;

@end

@implementation TrackingPriceMediator

- (instancetype)initWithBrowserState:(ChromeBrowserState*)browser_state {
  self = [super init];
  if (self) {
    _browserState = browser_state->AsWeakPtr();
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
    _mobileNotificationItem.on =
        [self prefValueForClient:PushNotificationClientId::kCommerce];
  }

  return _mobileNotificationItem;
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
  if (_consumer == consumer)
    return;
  _consumer = consumer;
  [_consumer setMobileNotificationItem:self.mobileNotificationItem];
  [_consumer setTrackPriceHeaderItem:self.trackPriceHeaderItem];
}

#pragma mark - TrackingPriceViewControllerDelegate

- (void)toggleSwitchItem:(TableViewItem*)item withValue:(BOOL)value {
  ItemType type = static_cast<ItemType>(item.type);
  switch (type) {
    case ItemTypeMobileNotifications: {
      self.mobileNotificationItem.on = value;
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
    default:
      // Not a switch.
      NOTREACHED();
      break;
  }
}

#pragma mark - Private

// Returns whether the push notification enabled feature's, `client_id`,
// permission status is enabled or disabled for the current user.
- (BOOL)prefValueForClient:(PushNotificationClientId)clientID {
  ChromeBrowserState* browser_state = _browserState.get();

  if (!browser_state) {
    return NO;
  }

  const PushNotificationAccountContext* account_context =
      PushNotificationBrowserStateServiceFactory::GetForBrowserState(
          browser_state)
          ->GetAccountContext();
  return
      [account_context.preferenceMap[@(static_cast<int>(clientID)).stringValue]
          boolValue];
}

@end

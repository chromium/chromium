// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/price_notifications/tracking_price/tracking_price_mediator.h"

#import "base/mac/foundation_util.h"
#import "base/notreached.h"
#import "ios/chrome/browser/ui/list_model/list_model.h"
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

@interface TrackingPriceMediator ()

// Mobile notification item.
@property(nonatomic, strong) TableViewSwitchItem* mobileNotificationItem;
// Header item.
@property(nonatomic, strong)
    TableViewLinkHeaderFooterItem* trackPriceHeaderItem;

@end

@implementation TrackingPriceMediator

#pragma mark - Properties

- (TableViewSwitchItem*)mobileNotificationItem {
  if (!_mobileNotificationItem) {
    _mobileNotificationItem =
        [[TableViewSwitchItem alloc] initWithType:ItemTypeMobileNotifications];
    _mobileNotificationItem.text = l10n_util::GetNSString(
        IDS_IOS_TRACKING_PRICE_MOBILE_NOTIFICATIONS_TITLE);
    _mobileNotificationItem.accessibilityIdentifier =
        kSettingsTrackingPriceMobileNotificationsCellId;
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
    case ItemTypeMobileNotifications:
      self.mobileNotificationItem.on = value;
      break;
    default:
      // Not a switch.
      NOTREACHED();
      break;
  }
}

@end

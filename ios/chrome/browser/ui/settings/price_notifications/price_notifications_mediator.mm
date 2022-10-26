// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/price_notifications/price_notifications_mediator.h"

#import "base/notreached.h"
#import "ios/chrome/browser/ui/icons/symbols.h"
#import "ios/chrome/browser/ui/list_model/list_model.h"
#import "ios/chrome/browser/ui/settings/price_notifications/price_notifications_constants.h"
#import "ios/chrome/browser/ui/settings/price_notifications/price_notifications_consumer.h"
#import "ios/chrome/browser/ui/settings/price_notifications/price_notifications_navigation_commands.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

NSString* const kTrackingPriceImageName = @"line_downtrend";

}  // namespace

// List of items.
typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeTrackingPrice = kItemTypeEnumZero,
};

@interface PriceNotificationsMediator ()

// All the items for the price notifications section.
@property(nonatomic, strong, readonly) TableViewItem* priceTrackingItem;

@end

@implementation PriceNotificationsMediator

@synthesize priceTrackingItem = _priceTrackingItem;

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
                      iconImage:kTrackingPriceImageName
        accessibilityIdentifier:kSettingsPriceNotificationsPriceTrackingCellId];
  }
  return _priceTrackingItem;
}

- (void)setConsumer:(id<PriceNotificationsConsumer>)consumer {
  if (_consumer == consumer)
    return;
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
                                     iconImage:(NSString*)imageName
                       accessibilityIdentifier:
                           (NSString*)accessibilityIdentifier {
  TableViewDetailIconItem* detailItem =
      [[TableViewDetailIconItem alloc] initWithType:type];
  detailItem.text = text;
  detailItem.detailText = detailText;
  detailItem.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  detailItem.accessibilityTraits |= UIAccessibilityTraitButton;
  detailItem.accessibilityIdentifier = accessibilityIdentifier;
  if (UseSymbols()) {
    detailItem.iconImage = CustomSettingsRootSymbol(symbol);
    detailItem.iconTintColor = UIColor.whiteColor;
    detailItem.iconCornerRadius = kColorfulBackgroundSymbolCornerRadius;
    detailItem.iconBackgroundColor = backgroundColor;
  } else {
    detailItem.iconImage = [UIImage imageNamed:imageName];
  }

  return detailItem;
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

@end

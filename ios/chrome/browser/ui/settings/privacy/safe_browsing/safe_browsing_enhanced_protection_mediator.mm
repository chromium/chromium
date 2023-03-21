// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/safe_browsing/safe_browsing_enhanced_protection_mediator.h"

#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/ui/icons/symbols.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_item.h"
#import "ios/chrome/browser/ui/settings/privacy/safe_browsing/safe_browsing_constants.h"
#import "ios/chrome/browser/ui/settings/privacy/safe_browsing/safe_browsing_enhanced_protection_consumer.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_google_chrome_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using ItemArray = NSArray<TableViewItem*>*;

namespace {

// List of item types.
typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeShieldIcon = kItemTypeEnumZero,
  ItemTypeGIcon,
  ItemTypeGlobeIcon,
  ItemTypeKeyIcon,
  ItemTypeMetricIcon,
};

// The size of the symbols.
const CGFloat kSymbolSize = 20;

}  // namespace

@interface SafeBrowsingEnhancedProtectionMediator ()

// All the items for the enhanced safe browsing section.
@property(nonatomic, strong, readonly)
    ItemArray safeBrowsingEnhancedProtectionItems;

@end

@implementation SafeBrowsingEnhancedProtectionMediator

@synthesize safeBrowsingEnhancedProtectionItems =
    _safeBrowsingEnhancedProtectionItems;

#pragma mark - Properties

- (ItemArray)safeBrowsingEnhancedProtectionItems {
  if (!_safeBrowsingEnhancedProtectionItems) {
    NSMutableArray* items = [NSMutableArray array];
    UIImage* shieldIcon =
        CustomSymbolWithPointSize(kPrivacySymbol, kSymbolSize);
    SettingsImageDetailTextItem* shieldIconItem = [self
             detailItemWithType:ItemTypeShieldIcon
                     detailText:
                         IDS_IOS_SAFE_BROWSING_ENHANCED_PROTECTION_BULLET_ONE
                          image:shieldIcon
        accessibilityIdentifier:kSafeBrowsingEnhancedProtectionShieldCellId];
    [items addObject:shieldIconItem];

#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
    UIImage* gIcon =
        CustomSymbolWithPointSize(kGoogleShieldSymbol, kSymbolSize);
#else
    UIImage* gIcon = DefaultSymbolWithPointSize(kInfoCircleSymbol, kSymbolSize);
#endif
    SettingsImageDetailTextItem* gIconItem = [self
             detailItemWithType:ItemTypeGIcon
                     detailText:
                         IDS_IOS_SAFE_BROWSING_ENHANCED_PROTECTION_BULLET_TWO
                          image:gIcon
        accessibilityIdentifier:kSafeBrowsingEnhancedProtectionGIconCellId];
    [items addObject:gIconItem];

    UIImage* globeIcon;
    if (@available(iOS 15, *)) {
      globeIcon = DefaultSymbolWithPointSize(kGlobeAmericasSymbol, kSymbolSize);
    } else {
      globeIcon = DefaultSymbolWithPointSize(kGlobeSymbol, kSymbolSize);
    }
    SettingsImageDetailTextItem* globeIconItem = [self
             detailItemWithType:ItemTypeGlobeIcon
                     detailText:
                         IDS_IOS_SAFE_BROWSING_ENHANCED_PROTECTION_BULLET_THREE
                          image:globeIcon
        accessibilityIdentifier:kSafeBrowsingEnhancedProtectionGlobeCellId];
    [items addObject:globeIconItem];

    UIImage* keyIcon = CustomSymbolWithPointSize(kPasswordSymbol, kSymbolSize);
    SettingsImageDetailTextItem* keyIconItem = [self
             detailItemWithType:ItemTypeKeyIcon
                     detailText:
                         IDS_IOS_SAFE_BROWSING_ENHANCED_PROTECTION_BULLET_FOUR
                          image:keyIcon
        accessibilityIdentifier:kSafeBrowsingEnhancedProtectionKeyCellId];
    [items addObject:keyIconItem];

    UIImage* metricIcon =
        DefaultSymbolWithPointSize(kCheckmarkCircleSymbol, kSymbolSize);
    SettingsImageDetailTextItem* metricIconItem = [self
             detailItemWithType:ItemTypeMetricIcon
                     detailText:
                         IDS_IOS_SAFE_BROWSING_ENHANCED_PROTECTION_BULLET_FIVE
                          image:metricIcon
        accessibilityIdentifier:kSafeBrowsingEnhancedProtectionMetricCellId];
    [items addObject:metricIconItem];

    _safeBrowsingEnhancedProtectionItems = items;
  }
  return _safeBrowsingEnhancedProtectionItems;
}

- (void)setConsumer:(id<SafeBrowsingEnhancedProtectionConsumer>)consumer {
  if (_consumer == consumer)
    return;
  _consumer = consumer;
  [_consumer setSafeBrowsingEnhancedProtectionItems:
                 self.safeBrowsingEnhancedProtectionItems];
}

#pragma mark - Private

// Creates item that will show what Enhanced Protection entails.
- (SettingsImageDetailTextItem*)detailItemWithType:(NSInteger)type
                                        detailText:(NSInteger)detailText
                                             image:(UIImage*)image
                           accessibilityIdentifier:
                               (NSString*)accessibilityIdentifier {
  SettingsImageDetailTextItem* detailItem =
      [[SettingsImageDetailTextItem alloc] initWithType:type];
  detailItem.detailText = l10n_util::GetNSString(detailText);
  detailItem.image = image;
  detailItem.imageViewTintColor = [UIColor colorNamed:kGrey600Color];
  detailItem.accessibilityIdentifier = accessibilityIdentifier;

  return detailItem;
}

@end

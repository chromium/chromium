// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/safe_browsing/safe_browsing_standard_protection_mediator.h"

#include "base/notreached.h"
#include "components/prefs/pref_service.h"
#import "ios/chrome/browser/ui/list_model/list_model.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_item.h"
#import "ios/chrome/browser/ui/settings/privacy/safe_browsing/safe_browsing_constants.h"
#import "ios/chrome/browser/ui/settings/privacy/safe_browsing/safe_browsing_standard_protection_consumer.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#include "ios/chrome/grit/ios_google_chrome_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using ItemArray = NSArray<TableViewItem*>*;

namespace {
// List of item types.
typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeShieldIcon = kItemTypeEnumZero,
  ItemTypeMetricIcon,
};
}  // namespace

@interface SafeBrowsingStandardProtectionMediator ()

// User pref service used to check if a specific pref is managed by enterprise
// policies.
@property(nonatomic, assign, readonly) PrefService* userPrefService;

// Local pref service used to check if a specific pref is managed by enterprise
// policies.
@property(nonatomic, assign, readonly) PrefService* localPrefService;

// All the items for the standard safe browsing section.
@property(nonatomic, strong, readonly)
    ItemArray safeBrowsingStandardProtectionItems;

@end

@implementation SafeBrowsingStandardProtectionMediator

@synthesize safeBrowsingStandardProtectionItems =
    _safeBrowsingStandardProtectionItems;

- (instancetype)initWithUserPrefService:(PrefService*)userPrefService
                       localPrefService:(PrefService*)localPrefService {
  self = [super init];
  if (self) {
    DCHECK(userPrefService);
    DCHECK(localPrefService);
    _userPrefService = userPrefService;
    _localPrefService = localPrefService;
  }
  return self;
}

#pragma mark - Properties

- (ItemArray)safeBrowsingStandardProtectionItems {
  if (!_safeBrowsingStandardProtectionItems) {
    NSMutableArray* items = [NSMutableArray array];
    SettingsImageDetailTextItem* shieldIconItem = [self
             detailItemWithType:ItemTypeShieldIcon
                     detailText:
                         IDS_IOS_SAFE_BROWSING_STANDARD_PROTECTION_BULLET_ONE
                          image:[[UIImage imageNamed:@"shield"]
                                    imageWithRenderingMode:
                                        UIImageRenderingModeAlwaysTemplate]
        accessibilityIdentifier:kSafeBrowsingStandardProtectionShieldCellId];
    [items addObject:shieldIconItem];

    SettingsImageDetailTextItem* metricIconItem = [self
             detailItemWithType:ItemTypeMetricIcon
                     detailText:
                         IDS_IOS_SAFE_BROWSING_STANDARD_PROTECTION_BULLET_TWO
                          image:[[UIImage imageNamed:@"bar_chart"]
                                    imageWithRenderingMode:
                                        UIImageRenderingModeAlwaysTemplate]
        accessibilityIdentifier:kSafeBrowsingStandardProtectionMetricCellId];
    [items addObject:metricIconItem];

    _safeBrowsingStandardProtectionItems = items;
  }
  return _safeBrowsingStandardProtectionItems;
}

- (void)setConsumer:(id<SafeBrowsingStandardProtectionConsumer>)consumer {
  if (_consumer == consumer)
    return;
  _consumer = consumer;
  [_consumer setSafeBrowsingStandardProtectionItems:
                 self.safeBrowsingStandardProtectionItems];
}

#pragma mark - Private

// Creates item in Standard Protection view.
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

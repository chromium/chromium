// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/shop_card/ui/shop_card_item.h"

#import "ios/chrome/browser/content_suggestions/public/content_suggestions_constants.h"

@implementation ShopCardItem

#pragma mark - NSCopying

- (instancetype)copyWithZone:(NSZone*)zone {
  ShopCardItem* item = [[super copyWithZone:zone] init];
  item.shopCardData = self.shopCardData;
  item.shopCardHandler = self.shopCardHandler;
  return item;
}

#pragma mark - MagicStackModule

- (ContentSuggestionsModuleType)type {
  return ContentSuggestionsModuleType::kShopCard;
}

@end

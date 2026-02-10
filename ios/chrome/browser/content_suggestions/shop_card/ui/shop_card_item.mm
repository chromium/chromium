// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/shop_card/ui/shop_card_item.h"

#import "ios/chrome/browser/content_suggestions/public/content_suggestions_constants.h"

@implementation ShopCardItem

#pragma mark - MagicStackModule

- (ContentSuggestionsModuleType)type {
  return ContentSuggestionsModuleType::kShopCard;
}

#pragma mark - NSCopying

- (id)copyWithZone:(NSZone*)zone {
  ShopCardItem* copy = [[super copyWithZone:zone] init];
  copy.shopCardData = self.shopCardData;
  copy.shopCardFaviconConsumerSource = self.shopCardFaviconConsumerSource;
  copy.commandHandler = self.commandHandler;
  return copy;
}

@end

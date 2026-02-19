// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/shop_card/ui/shop_card_config.h"

#import "ios/chrome/browser/content_suggestions/public/content_suggestions_constants.h"

@implementation ShopCardConfig

#pragma mark - NSCopying

- (instancetype)copyWithZone:(NSZone*)zone {
  ShopCardConfig* config = [[super copyWithZone:zone] init];
  // The updates to properties must be reflected in the copy method.
  // LINT.IfChange(Copy)
  config.shopCardData = self.shopCardData;
  config.shopCardHandler = self.shopCardHandler;
  // LINT.ThenChange(shop_card_config.h:Copy)
  return config;
}

#pragma mark - MagicStackModule

- (ContentSuggestionsModuleType)type {
  return ContentSuggestionsModuleType::kShopCard;
}

- (BOOL)shouldShowSeeMore {
  return YES;
}

@end

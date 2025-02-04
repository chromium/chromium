// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/shop_card/shop_card_item.h"

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"

@implementation ShopCardItem

#pragma mark - MagicStackModule

- (ContentSuggestionsModuleType)type {
  return ContentSuggestionsModuleType::kShopCard;
}

@end

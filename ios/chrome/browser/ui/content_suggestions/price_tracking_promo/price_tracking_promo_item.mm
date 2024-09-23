// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/price_tracking_promo/price_tracking_promo_item.h"

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"

@implementation PriceTrackingPromoItem

#pragma mark - MagicStackModule

- (ContentSuggestionsModuleType)type {
  return ContentSuggestionsModuleType::kPriceTrackingPromo;
}

@end

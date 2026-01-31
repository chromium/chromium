// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/price_tracking_promo/ui/price_tracking_promo_item.h"

#import "ios/chrome/browser/content_suggestions/price_tracking_promo/public/price_tracking_promo_constants.h"
#import "ios/chrome/browser/content_suggestions/public/content_suggestions_constants.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@implementation PriceTrackingPromoItem

#pragma mark - MagicStackModule

- (ContentSuggestionsModuleType)type {
  return ContentSuggestionsModuleType::kPriceTrackingPromo;
}

#pragma mark - StandaloneModuleViewConfiguration

- (UIImage*)productImage {
  return [UIImage imageWithData:self.productImageData
                          scale:[UIScreen mainScreen].scale];
}

- (UIImage*)fallbackSymbolImage {
  return CustomSymbolWithPointSize(kDownTrendSymbol, 10);
}

- (NSString*)titleText {
  return l10n_util::GetNSString(
      IDS_IOS_CONTENT_SUGGESTIONS_PRICE_TRACKING_PROMO_TITLE);
}

- (NSString*)bodyText {
  return l10n_util::GetNSString(
      IDS_IOS_CONTENT_SUGGESTIONS_PRICE_TRACKING_PROMO_DESCRIPTION);
}

- (NSString*)buttonText {
  return l10n_util::GetNSString(
      IDS_IOS_CONTENT_SUGGESTIONS_PRICE_TRACKING_PROMO_ALLOW);
}

- (NSString*)accessibilityIdentifier {
  return kPriceTrackingPromoViewID;
}

@end

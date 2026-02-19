// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/price_tracking_promo/ui/price_tracking_promo_config.h"

#import "base/check_op.h"
#import "ios/chrome/browser/content_suggestions/price_tracking_promo/public/price_tracking_promo_constants.h"
#import "ios/chrome/browser/content_suggestions/price_tracking_promo/ui/price_tracking_promo_commands.h"
#import "ios/chrome/browser/content_suggestions/public/content_suggestions_constants.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

using l10n_util::GetNSString;

namespace {

// Size of the fallback symbol image.
constexpr CGFloat kFallbackSymbolSize = 10;

}  // namespace

@implementation PriceTrackingPromoConfig

#pragma mark - NSCopying

- (instancetype)copyWithZone:(NSZone*)zone {
  PriceTrackingPromoConfig* config = [[super copyWithZone:zone] init];
  // The updates to properties must be reflected in the copy method.
  // LINT.IfChange(Copy)
  config.priceTrackingPromoHandler = self.priceTrackingPromoHandler;
  config.productImageData = self.productImageData;
  // LINT.ThenChange(price_tracking_promo_config.h:Copy)
  return config;
}

#pragma mark - MagicStackModule

- (ContentSuggestionsModuleType)type {
  return ContentSuggestionsModuleType::kPriceTrackingPromo;
}

#pragma mark - StandaloneModuleViewConfig

- (UIImage*)productImage {
  return [UIImage imageWithData:self.productImageData
                          scale:[UIScreen mainScreen].scale];
}

- (UIImage*)fallbackSymbolImage {
  return CustomSymbolWithPointSize(kDownTrendSymbol, kFallbackSymbolSize);
}

- (NSString*)titleText {
  return GetNSString(IDS_IOS_CONTENT_SUGGESTIONS_PRICE_TRACKING_PROMO_TITLE);
}

- (NSString*)bodyText {
  return GetNSString(
      IDS_IOS_CONTENT_SUGGESTIONS_PRICE_TRACKING_PROMO_DESCRIPTION);
}

- (NSString*)buttonText {
  return GetNSString(IDS_IOS_CONTENT_SUGGESTIONS_PRICE_TRACKING_PROMO_ALLOW);
}

- (NSString*)accessibilityIdentifier {
  return kPriceTrackingPromoViewID;
}

#pragma mark - StandaloneModuleViewTapDelegate

- (void)buttonTappedForModuleType:(ContentSuggestionsModuleType)moduleType {
  CHECK_EQ(moduleType, ContentSuggestionsModuleType::kPriceTrackingPromo);
  [self.priceTrackingPromoHandler allowPriceTrackingNotifications];
}

@end

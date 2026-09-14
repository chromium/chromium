// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_browser/promo/contextual/coordinator/contextual_default_browser_promo_mediator.h"

#import "base/i18n/rtl.h"
#import "ios/chrome/browser/default_browser/promo/contextual/ui/contextual_default_browser_promo_consumer.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Animation asset names for Gemini promo.
NSString* const kGeminiAnimationAssetName = @"FRE_Summarize_Slide";
NSString* const kGeminiAnimationAssetNameRTL = @"FRE_Summarize_Slide_RTL";

}  // namespace

@implementation ContextualDefaultBrowserPromoMediator {
  ContextualDefaultBrowserPromoType _promoType;
}

#pragma mark - Initializers

- (instancetype)initWithPromoType:(ContextualDefaultBrowserPromoType)promoType {
  self = [super init];
  if (self) {
    _promoType = promoType;
  }
  return self;
}

#pragma mark - Properties

- (void)setConsumer:(id<ContextualDefaultBrowserPromoConsumer>)consumer {
  _consumer = consumer;
  if (!_consumer) {
    return;
  }

  switch (_promoType) {
    case ContextualDefaultBrowserPromoType::kGemini:
      [_consumer
          setPromoTitle:l10n_util::GetNSString(
                            IDS_IOS_DEFAULT_BROWSER_CONTEXTUAL_GEMINI_TITLE)];
      [_consumer setPromoSubtitle:
                     l10n_util::GetNSString(
                         IDS_IOS_DEFAULT_BROWSER_CONTEXTUAL_GEMINI_SUBTITLE)];
      [_consumer setAnimationAssetName:base::i18n::IsRTL()
                                           ? kGeminiAnimationAssetNameRTL
                                           : kGeminiAnimationAssetName];
      [_consumer
          setLightModeColorProvider:SummarizeSlideLightModeColorProvider()
              darkModeColorProvider:SummarizeSlideDarkModeColorProvider()];
      break;
  }
}

#pragma mark - Public

- (void)disconnect {
  _consumer = nil;
}

@end

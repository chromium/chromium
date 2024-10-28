// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/tips/tips_module_state.h"

#import "components/segmentation_platform/embedder/home_modules/tips_manager/constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/tips/tips_module_audience.h"

using segmentation_platform::TipIdentifier;

@implementation TipsModuleState

- (instancetype)initWithIdentifier:(TipIdentifier)identifier {
  if ((self = [super init])) {
    _identifier = identifier;
  }

  return self;
}

#pragma mark - MagicStackModule

- (ContentSuggestionsModuleType)type {
  CHECK(IsTipsMagicStackEnabled());

  if (_identifier == TipIdentifier::kLensShop &&
      TipsLensShopExperimentTypeEnabled() ==
          TipsLensShopExperimentType::kWithProductImage &&
      _productImageData.length > 0) {
    return ContentSuggestionsModuleType::kTipsWithProductImage;
  }

  return ContentSuggestionsModuleType::kTips;
}

@end

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/tips/tips_module_state.h"

#import "components/segmentation_platform/embedder/home_modules/tips_manager/constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"

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
  // Display the Tips module (with a product image) if the product image URL is
  // valid and reachable.
  if (_identifier == TipIdentifier::kLensShop) {
    NSError* err;
    [self.productImageURL checkResourceIsReachableAndReturnError:&err];

    // Verify the product image URL is reachable.
    if (!err) {
      return ContentSuggestionsModuleType::kTipsWithProductImage;
    }
  }

  return ContentSuggestionsModuleType::kTips;
}

@end

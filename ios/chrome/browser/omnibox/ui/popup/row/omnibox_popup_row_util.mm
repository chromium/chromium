// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/ui/popup/row/omnibox_popup_row_util.h"

#import "base/check.h"
#import "ios/chrome/browser/omnibox/public/omnibox_ui_features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ui/base/device_form_factor.h"

namespace {
// Multiplier values for supported content sizes.
const double kContentSizeMultiplierXS = 0.8;
const double kContentSizeMultiplierS = 0.9;
const double kContentSizeMultiplierM = 1.0;
const double kContentSizeMultiplierL = 1.2;
const double kContentSizeMultiplierXL = 1.4;
const double kContentSizeMultiplier2XL = 1.6;
const double kContentSizeMultiplier3XL = 1.8;
// Single maximum zoom level for accessibility. This value is only slightly
// higher than the 3XL zoom avoid visually breaking the UI.
const double kContentSizeMultiplierAccesibility = 2.0;
}  // namespace

BOOL ShouldApplyOmniboxPopoutLayout(UITraitCollection* traitCollection) {
  return ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET &&
         IsRegularXRegularSizeClass(traitCollection);
}

CGFloat OmniboxPopupRowContentSizeMultiplierForCategory(
    UIContentSizeCategory category) {
  NSDictionary<NSString*, NSNumber*>* sizeMapping = @{
    UIContentSizeCategoryExtraSmall : @(kContentSizeMultiplierXS),
    UIContentSizeCategorySmall : @(kContentSizeMultiplierS),
    UIContentSizeCategoryMedium : @(kContentSizeMultiplierM),
    UIContentSizeCategoryLarge : @(kContentSizeMultiplierL),
    UIContentSizeCategoryExtraLarge : @(kContentSizeMultiplierXL),
    UIContentSizeCategoryExtraExtraLarge : @(kContentSizeMultiplier2XL),
    UIContentSizeCategoryExtraExtraExtraLarge : @(kContentSizeMultiplier3XL),
    UIContentSizeCategoryAccessibilityMedium :
        @(kContentSizeMultiplierAccesibility),
    UIContentSizeCategoryAccessibilityLarge :
        @(kContentSizeMultiplierAccesibility),
    UIContentSizeCategoryAccessibilityExtraLarge :
        @(kContentSizeMultiplierAccesibility),
    UIContentSizeCategoryAccessibilityExtraExtraLarge :
        @(kContentSizeMultiplierAccesibility),
    UIContentSizeCategoryAccessibilityExtraExtraExtraLarge :
        @(kContentSizeMultiplierAccesibility),
  };

  return [sizeMapping[category] doubleValue];
}

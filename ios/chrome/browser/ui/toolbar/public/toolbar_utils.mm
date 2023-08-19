// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/public/toolbar_utils.h"

#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ios/components/ui_util/dynamic_type_util.h"

namespace {

// Returns the `category` unchanged, unless it is ||, in which case it returns
// the preferred content size category from the shared application.
UIContentSizeCategory NormalizedCategory(UIContentSizeCategory category) {
  if ([category isEqualToString:UIContentSizeCategoryUnspecified])
    return [UIApplication sharedApplication].preferredContentSizeCategory;
  return category;
}

// Returns an interpolation of the height based on the multiplier associated
// with `category`, clamped between UIContentSizeCategoryLarge and
// UIContentSizeCategoryAccessibilityExtraLarge. This multiplier is applied to
// `default_height` - `non_dynamic_height`.
CGFloat Interpolate(UIContentSizeCategory category,
                    CGFloat default_height,
                    CGFloat non_dynamic_height) {
  return AlignValueToPixel((default_height - non_dynamic_height) *
                               ToolbarClampedFontSizeMultiplier(category) +
                           non_dynamic_height);
}

}  // namespace

CGFloat ToolbarClampedFontSizeMultiplier(UIContentSizeCategory category) {
  return ui_util::SystemSuggestedFontSizeMultiplier(
      category, UIContentSizeCategoryLarge,
      UIContentSizeCategoryAccessibilityExtraLarge);
}

CGFloat ToolbarCollapsedHeight(UIContentSizeCategory category) {
  category = NormalizedCategory(category);
  return Interpolate(category, kToolbarHeightFullscreen,
                     kNonDynamicToolbarHeightFullscreen);
}

CGFloat ToolbarExpandedHeight(UIContentSizeCategory category) {
  category = NormalizedCategory(category);
  return Interpolate(category, kToolbarOmniboxHeight, kNonDynamicToolbarHeight);
}

CGFloat LocationBarHeight(UIContentSizeCategory category) {
  category = NormalizedCategory(category);
  CGFloat verticalMargin =
      2 * kAdaptiveLocationBarVerticalMargin - kTopToolbarUnsplitMargin;
  CGFloat dynamicTypeVerticalAdjustment =
      (ToolbarClampedFontSizeMultiplier(category) - 1) *
      (kLocationBarVerticalMarginDynamicType +
       kAdaptiveLocationBarVerticalMargin);
  verticalMargin = verticalMargin + dynamicTypeVerticalAdjustment;
  return AlignValueToPixel(ToolbarExpandedHeight(category) - verticalMargin);
}

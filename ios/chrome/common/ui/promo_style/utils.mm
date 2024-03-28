// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/promo_style/utils.h"

#import "ios/chrome/common/ui/util/device_util.h"

namespace {

// Whether the `traitCollection` has a regular vertical and regular horizontal
// size class.
// TODO(crbug.com/330745268): This method should be deduplicate with the version
// in ios/chrome/browser/shared/ui/util/uikit_ui_util.h.
bool IsRegularXRegularSizeClass(UITraitCollection* traitCollection) {
  return traitCollection.verticalSizeClass == UIUserInterfaceSizeClassRegular &&
         traitCollection.horizontalSizeClass == UIUserInterfaceSizeClassRegular;
}

}  // namespace

UIFontTextStyle GetTitleLabelFontTextStyle(UIViewController* view_controller) {
  UIViewController* presenter = view_controller.presentingViewController
                                    ? view_controller.presentingViewController
                                    : view_controller;
  BOOL accessibility_category = UIContentSizeCategoryIsAccessibilityCategory(
      presenter.traitCollection.preferredContentSizeCategory);
  if (!accessibility_category) {
    if (IsRegularXRegularSizeClass(presenter.traitCollection)) {
      return UIFontTextStyleTitle1;
    } else if (!IsSmallDevice()) {
      return UIFontTextStyleLargeTitle;
    }
  }
  return UIFontTextStyleTitle2;
}

UIFont* GetFRETitleFont(UIFontTextStyle text_style) {
  UIFontDescriptor* descriptor =
      [UIFontDescriptor preferredFontDescriptorWithTextStyle:text_style];
  UIFont* font = [UIFont systemFontOfSize:descriptor.pointSize
                                   weight:UIFontWeightBold];
  UIFontMetrics* font_metrics = [UIFontMetrics metricsForTextStyle:text_style];
  return [font_metrics scaledFontForFont:font];
}

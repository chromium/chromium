// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safari_data_import/public/ui_utils.h"

#import "ios/chrome/common/ui/util/ui_util.h"

UIFontTextStyle GetSafariDataImportTitleLabelFontTextStyle(
    UITraitCollection* traitCollection) {
  BOOL accessibility_category = UIContentSizeCategoryIsAccessibilityCategory(
      traitCollection.preferredContentSizeCategory);
  if (accessibility_category) {
    return UIFontTextStyleTitle2;
  }
  if (IsRegularXRegularSizeClass(traitCollection)) {
    return UIFontTextStyleTitle1;
  }
  return UIFontTextStyleLargeTitle;
}

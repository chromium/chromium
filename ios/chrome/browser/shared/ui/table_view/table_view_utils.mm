// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"

#import "ios/chrome/browser/shared/ui/table_view/table_view_constants.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ui/base/device_form_factor.h"

namespace {

// Default header Height when none is set.
const CGFloat kDefaultHeaderHeight = 10;

// Whether the style used by the TableView should have insets.
bool HasTableViewInsetStyle() {
  return ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_PHONE ||
         !UIContentSizeCategoryIsAccessibilityCategory(
             UIApplication.sharedApplication.preferredContentSizeCategory);
}

}  // namespace

UITableViewStyle ChromeTableViewStyle() {
  if (HasTableViewInsetStyle()) {
    return UITableViewStyleInsetGrouped;
  }
  return UITableViewStyleGrouped;
}

CGFloat ChromeTableViewHeightForHeaderInSection(NSInteger section) {
  if (section == 0) {
    return kTableViewFirstHeaderHeight;
  }

  return kDefaultHeaderHeight;
}

CGFloat ChromeTableViewHorizontalPadding() {
  if (HasTableViewInsetStyle()) {
    return 0;
  }
  return kTableViewHorizontalSpacing;
}

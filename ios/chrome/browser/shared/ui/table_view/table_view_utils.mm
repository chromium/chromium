// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"

#import "ios/chrome/common/ui/util/device_util.h"

namespace {

// Header height used as a pading between the first cell and the navigation bar.
const CGFloat kFirstHeaderHeight = 25.0;

// Default header Height when none is set.
const CGFloat kDefaultHeaderHeight = 10;

}  // namespace

UITableViewStyle ChromeTableViewStyle() {
  if (!IsSmallDevice()) {
    return UITableViewStyleInsetGrouped;
  }

  return UITableViewStyleGrouped;
}

CGFloat ChromeTableViewHeightForHeaderInSection(NSInteger section) {
  if (section == 0) {
    return kFirstHeaderHeight;
  }

  return kDefaultHeaderHeight;
}

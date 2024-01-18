// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_TABLE_VIEW_UTILS_H_
#define IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_TABLE_VIEW_UTILS_H_

#import <UIKit/UIKit.h>

// Returns an UITableViewStyle according to the width of the current device.
UITableViewStyle ChromeTableViewStyle();

// Returns a header height according to the given section.
// The returned size for the first section is bigger because it's used as
// padding between the first cell and the navigation bar.
CGFloat ChromeTableViewHeightForHeaderInSection(NSInteger section);

template <typename T>
T* DequeueTableViewCell(UITableView* table_view) {
  return [table_view
      dequeueReusableCellWithIdentifier:NSStringFromClass([T class])];
}

template <typename T>
T* DequeueTableViewHeaderFooter(UITableView* table_view) {
  return [table_view
      dequeueReusableHeaderFooterViewWithIdentifier:NSStringFromClass(
                                                        [T class])];
}

template <typename T>
void RegisterTableViewCell(UITableView* table_view) {
  [table_view registerClass:[T class]
      forCellReuseIdentifier:NSStringFromClass([T class])];
}

template <typename T>
void RegisterTableViewHeaderFooter(UITableView* table_view) {
  [table_view registerClass:[T class]
      forHeaderFooterViewReuseIdentifier:NSStringFromClass([T class])];
}

#endif  // IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_TABLE_VIEW_UTILS_H_

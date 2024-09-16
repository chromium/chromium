// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUPS_EG_UTILS_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUPS_EG_UTILS_H_

#import <Foundation/Foundation.h>

namespace chrome_test_util {

// Creates a tab group named `group_name` with the tab item at `index`.
// Set `first_group` to false when creating a subsequent group, as the context
// menu labels are different and EarlGrey needs to know exactly which labels to
// select.
// Requires the UI to be located in the Tab Groups panel.
void CreateTabGroupAtIndex(int index,
                           NSString* group_name,
                           bool first_group = true);

}  // namespace chrome_test_util

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUPS_EG_UTILS_H_

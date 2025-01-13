// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_TAB_GROUPS_TAB_GROUP_INDICATOR_FEATURES_UTILS_H_
#define IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_TAB_GROUPS_TAB_GROUP_INDICATOR_FEATURES_UTILS_H_

// Feature parameters for the tab group indicator.
extern const char kTabGroupIndicatorVisible[];
extern const char kTabGroupIndicatorBelowOmnibox[];
extern const char kTabGroupIndicatorButtonsUpdate[];

// Whether the tab group indicator is visible.
bool HasTabGroupIndicatorVisible();

// Whether the tab group indicator is below the omnibox.
bool HasTabGroupIndicatorBelowOmnibox();

// Whether the grid and the group view buttons are updated when the tab group
// indicator feature is enabled.
bool HasTabGroupIndicatorButtonsUpdated();

#endif  // IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_TAB_GROUPS_TAB_GROUP_INDICATOR_FEATURES_UTILS_H_

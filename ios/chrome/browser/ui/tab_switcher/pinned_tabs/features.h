// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_PINNED_TABS_FEATURES_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_PINNED_TABS_FEATURES_H_

#import "base/feature_list.h"

// Feature flag that enables Pinned Tabs.
BASE_DECLARE_FEATURE(kEnablePinnedTabs);

// Feature parameters for Pinned Tabs. If no parameter is set, the  default
// (bottom) position will be used.
extern const char kEnablePinnedTabsParameterName[];
extern const char kEnablePinnedTabsBottomParam[];
extern const char kEnablePinnedTabsTopParam[];

// Positions of the Pinned tabs.
enum PinnedTabsPosition {
  kBottomPosition = 0,
  kTopPosition,
};

// Convenience method for determining if Pinned Tabs is enabled.
bool IsPinnedTabsEnabled();

// Convenience method for determining the position of Pinned Tabs.
PinnedTabsPosition GetPinnedTabsPosition();

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_PINNED_TABS_FEATURES_H_

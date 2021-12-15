// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_FEATURES_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_FEATURES_H_

#include "base/feature_list.h"

// Feature flag to enable Tabs Search.
extern const base::Feature kTabsSearch;

// Whether the kTabsSearch flag is enabled.
bool IsTabsSearchEnabled();

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_FEATURES_H_

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_FEATURES_H_
#define IOS_CHROME_BROWSER_TABS_FEATURES_H_

#import <Foundation/Foundation.h>

#import "base/feature_list.h"

// Feature flag that enables Pinned Tabs.
BASE_DECLARE_FEATURE(kEnablePinnedTabs);

// Convenience method for determining if Pinned Tabs is enabled.
bool IsPinnedTabsEnabled();

#endif  // IOS_CHROME_BROWSER_TABS_FEATURES_H_

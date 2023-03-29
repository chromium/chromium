// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_FEATURES_H_
#define IOS_CHROME_BROWSER_TABS_FEATURES_H_

#import <Foundation/Foundation.h>

#import "base/feature_list.h"

// Feature flag that enables Pinned Tabs.
BASE_DECLARE_FEATURE(kEnablePinnedTabs);

// User default key used to determine if Pinned Tabs was used in the overflow
// menu.
extern NSString* const kPinnedTabsOverflowEntryKey;

// Feature parameter for Pinned Tabs.
extern const char kEnablePinnedTabsOverflowParam[];

// Convenience method for determining if Pinned Tabs is enabled.
bool IsPinnedTabsEnabled();

// Convenience method for determining if Pinned Tabs for the overflow menu is
// enabled.
bool IsPinnedTabsOverflowEnabled();

// Returns whether Pinned Tabs was used in the overflow menu.
bool WasPinnedTabOverflowUsed();

// Set that Pinned Tabs was used in the overflow menu.
void SetPinnedTabOverflowUsed();

#endif  // IOS_CHROME_BROWSER_TABS_FEATURES_H_

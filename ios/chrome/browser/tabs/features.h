// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_FEATURES_H_
#define IOS_CHROME_BROWSER_TABS_FEATURES_H_

#import <Foundation/Foundation.h>

// Convenience method for determining if Pinned Tabs is enabled.
// The Pinned Tabs feature is fully enabled on iPhone and disabled on iPad.
bool IsPinnedTabsEnabled();

#endif  // IOS_CHROME_BROWSER_TABS_FEATURES_H_

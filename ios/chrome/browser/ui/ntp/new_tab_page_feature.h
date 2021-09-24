// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_FEATURE_H_
#define IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_FEATURE_H_

#include "base/feature_list.h"

// Feature flag to enable showing a live preview for discover feed when opening
// the feed context menu.
extern const base::Feature kEnableDiscoverFeedPreview;

// Feature flag to enable improving the usage of memory of the NTP.
extern const base::Feature kEnableNTPMemoryEnhancement;

// Whether the discover feed content preview is shown in the context menu.
bool IsDiscoverFeedPreviewEnabled();

// Whether the discover feed appflows are enabled.
bool IsDiscoverFeedAppFlowsEnabled();

#endif  // IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_FEATURE_H_

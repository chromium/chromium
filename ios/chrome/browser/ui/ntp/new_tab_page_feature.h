// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_FEATURE_H_
#define IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_FEATURE_H_

#include "base/feature_list.h"

// Feature flag to enable showing a live preview for Discover feed when opening
// the feed context menu.
extern const base::Feature kEnableDiscoverFeedPreview;

// Feature flag to enable shorter cache so that more ghost cards appear.
extern const base::Feature kEnableDiscoverFeedShorterCache;

// Feature flag to enable static resource serving for the Discover feed.
extern const base::Feature kEnableDiscoverFeedStaticResourceServing;

// Feature flag to enable discofeed endpoint for the Discover feed.
extern const base::Feature kEnableDiscoverFeedDiscoFeedEndpoint;

// Feature flag to enable static resource serving for the Discover feed.
extern const base::Feature kEnableDiscoverFeedStaticResourceServing;

// A parameter to indicate whether Reconstructed Templates is enabled for static
// resource serving.
extern const char kDiscoverFeedSRSReconstructedTemplatesEnabled[];

// A parameter to indicate whether Preload Templates is enabled for static
// resource serving.
extern const char kDiscoverFeedSRSPreloadTemplatesEnabled[];

// Feature flag to enable the Following feed in the NTP.
// Use IsWebChannelsEnabled() instead of this constant directly.
// TODO(crbug.com/1264872): move it to web_channels feature directory when there
// has one since this feature will be used outside of NTP.
extern const base::Feature kEnableWebChannels;

// Feature flag to fix the NTP view hierarchy if it is broken before applying
// constraints.
// TODO(crbug.com/1262536): Remove this when it is fixed.
extern const base::Feature kNTPViewHierarchyRepair;

// Whether the Discover feed content preview is shown in the context menu.
bool IsDiscoverFeedPreviewEnabled();

// Whether the Discover feed appflows are enabled.
bool IsDiscoverFeedAppFlowsEnabled();

// Whether the Discover feed shorter cache is enabled.
bool IsDiscoverFeedShorterCacheEnabled();

// Whether the Following Feed is enabled on NTP.
bool IsWebChannelsEnabled();

// Whether the NTP view hierarchy repair is enabled.
bool IsNTPViewHierarchyRepairEnabled();

#endif  // IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_FEATURE_H_

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_FEATURE_H_
#define IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_FEATURE_H_

#include "base/feature_list.h"

// Feature flag to enable showing a live preview for Discover feed when opening
// the feed context menu.
extern const base::Feature kEnableDiscoverFeedPreview;

// Feature flag to show ghost cards when refreshing the discover feed.
extern const base::Feature kDiscoverFeedGhostCardsEnabled;

// Feature flag to enable static resource serving for the Discover feed.
extern const base::Feature kEnableDiscoverFeedStaticResourceServing;

// Feature flag to enable discofeed endpoint for the Discover feed.
extern const base::Feature kEnableDiscoverFeedDiscoFeedEndpoint;

// Feature flag to enable static resource serving for the Discover feed.
extern const base::Feature kEnableDiscoverFeedStaticResourceServing;

// Feature flag to enable the sync promo on top of the discover feed.
extern const base::Feature kEnableDiscoverFeedTopSyncPromo;

// A parameter to indicate whether Reconstructed Templates is enabled for static
// resource serving.
extern const char kDiscoverFeedSRSReconstructedTemplatesEnabled[];

// A parameter to indicate whether Preload Templates is enabled for static
// resource serving.
extern const char kDiscoverFeedSRSPreloadTemplatesEnabled[];

// A parameter to indicate the style used for the discover feed top promo.
extern const char kDiscoverFeedTopSyncPromoStyleParam[];

// A parameter value used for displaying the full with title promo style.
extern const char kDiscoverFeedTopSyncPromoStyleFullWithTitle[];

// A parameter value used for displaying the compact promo style.
extern const char kDiscoverFeedTopSyncPromoStyleCompact[];

// Feature flag to fix the NTP view hierarchy if it is broken before applying
// constraints.
// TODO(crbug.com/1262536): Remove this when it is fixed.
extern const base::Feature kNTPViewHierarchyRepair;

// Feature flag to remove the Feed from the NTP.
extern const base::Feature kEnableFeedAblation;

// Whether the Discover feed content preview is shown in the context menu.
bool IsDiscoverFeedPreviewEnabled();

// Whether the NTP view hierarchy repair is enabled.
bool IsNTPViewHierarchyRepairEnabled();

// Whether the Discover feed top sync promotion is enabled.
bool IsDiscoverFeedTopSyncPromoEnabled();

// Whether the feed top sync promotion is compact or not.
bool IsDiscoverFeedTopSyncPromoCompact();

// Whether the Discover feed ablation experiment is enabled.
bool IsFeedAblationEnabled();

// Whether the ghost cards should be shown when refreshing Discover feed
// content.
bool IsDiscoverFeedGhostCardsEnabled();

#endif  // IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_FEATURE_H_

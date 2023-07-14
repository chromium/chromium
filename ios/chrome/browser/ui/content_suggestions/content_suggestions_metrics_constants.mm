// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_metrics_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - Actions

const char kShowBookmarksAction[] = "MobileNTPShowBookmarks";
const char kShowReadingListAction[] = "MobileNTPShowReadingList";
const char kShowRecentTabsAction[] = "MobileNTPShowRecentTabs";
const char kShowHistoryAction[] = "MobileNTPShowHistory";
const char kShowWhatsNewAction[] = "MobileNTPShowWhatsNew";
const char kShowMostVisitedAction[] = "MobileNTPShowMostVisited";
const char kMostVisitedAction[] = "MobileNTPMostVisited";
const char kMostVisitedUrlBlacklistedAction[] = "MostVisited_UrlBlacklisted";
const char kShowReturnToRecentTabTileAction[] =
    "IOS.StartSurface.ShowReturnToRecentTabTile";
const char kOpenMostRecentTabAction[] = "IOS.StartSurface.OpenMostRecentTab";

#pragma mark - Histograms

const char kTrendingQueriesHistogram[] = "IOS.TrendingQueries";
const char kMagicStackTopModuleImpressionHistogram[] =
    "IOS.MagicStack.Module.TopImpression";
const char kMagicStackModuleEngagementHistogram[] =
    "IOS.MagicStack.Module.Click";

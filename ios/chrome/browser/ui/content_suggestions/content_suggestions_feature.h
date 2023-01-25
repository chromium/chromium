// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_FEATURE_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_FEATURE_H_

#include "base/feature_list.h"

// Store local state preference with whether the client has participated in
// kModularHomeTrendingQueriesClientSideFieldTrialName experiment or not.
extern const char kTrialPrefName[];

// The current trial version of
// kModularHomeTrendingQueriesClientSideFieldTrialName; should be updated when
// the experiment is modified.
extern const int kCurrentTrialVersion;

// Feature to choose between the old Zine feed or the new Discover feed in the
// Bling new tab page.
// Use IsDiscoverFeedEnabled() instead of this constant directly.
// TODO(crbug.com/1385512): Remove this.
BASE_DECLARE_FEATURE(kDiscoverFeedInNtp);

// Feature to use one NTP for all tabs in a Browser.
BASE_DECLARE_FEATURE(kSingleNtp);

// Feature to section the Content Suggestions into modules.
BASE_DECLARE_FEATURE(kContentSuggestionsUIModuleRefresh);
// Feature version of kContentSuggestionsUIModuleRefresh used for client-side
// study.
BASE_DECLARE_FEATURE(kContentSuggestionsUIModuleRefreshNewUser);

// Name of the field trial for when kContentSuggestionsUIModuleRefresh is
// enabled in about_flags.
extern const char
    kContentSuggestionsUIModuleRefreshFlagOverrideFieldTrialName[];

// Feature params for kContentSuggestionsUIModuleRefresh.
extern const char kContentSuggestionsUIModuleRefreshMinimizeSpacingParam[];
extern const char kContentSuggestionsUIModuleRefreshRemoveHeadersParam[];

// Feature to hide the MVT and Shortcuts tiles for new users.
BASE_DECLARE_FEATURE(kHideMVTAndShortcutsForNewUsers);

// Feature to show the Trending Queries module.
BASE_DECLARE_FEATURE(kTrendingQueriesModule);
// Feature version of kTrendingQueriesModule used for client-side study.
BASE_DECLARE_FEATURE(kTrendingQueriesModuleNewUser);

// Feature params for kTrendingQueriesModule.
extern const char kTrendingQueriesHideShortcutsParam[];
extern const char kTrendingQueriesDisabledFeedParam[];

// Name of the Modular Home + Trending Queries Client-side Field Trial.
extern const char kModularHomeTrendingQueriesClientSideFieldTrialName[];

// Name of the Trending Queries flag override field trial.
extern const char kTrendingQueriesFlagOverrideFieldTrialName[];

// A parameter to indicate whether the native UI is enabled for the discover
// feed.
// TODO(crbug.com/1385512): Remove this.
extern const char kDiscoverFeedIsNativeUIEnabled[];

// Whether the Discover feed is enabled instead of the Zine feed.
// TODO(crbug.com/1385512): Remove this.
bool IsDiscoverFeedEnabled();

// Whether the MVTs and Shortcuts should be hidden or not.
bool IsHideMVTAndShortcutsEnabled();

// Whether the Content Suggestions UI Module Refresh feature is enabled.
bool IsContentSuggestionsUIModuleRefreshEnabled();

// Whether some spacing should be removed for the Content Suggestions UI Module
// Refresh feature.
bool ShouldMinimizeSpacingForModuleRefresh();

// Whether the module header should not be shown for the Content Suggestions UI
// Module Refresh feature.
bool ShouldRemoveHeadersForModuleRefresh();

// Whether the Trending Queries module feature is enabled.
bool IsTrendingQueriesModuleEnabled();

// Whether the shorctus should be hidden while showing the Trending Queries
// module.
bool ShouldHideShortcutsForTrendingQueries();

// Whether the Trending Queries module should only be shown to users who had the
// feed disabled.
bool ShouldOnlyShowTrendingQueriesForDisabledFeed();

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_FEATURE_H_

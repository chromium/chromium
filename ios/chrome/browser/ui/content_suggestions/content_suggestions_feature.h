// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_FEATURE_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_FEATURE_H_

#include "base/feature_list.h"

// Feature to choose between the old Zine feed or the new Discover feed in the
// Bling new tab page.
// Use IsDiscoverFeedEnabled() instead of this constant directly.
extern const base::Feature kDiscoverFeedInNtp;

// Feature to use one NTP for all tabs in a Browser.
extern const base::Feature kSingleNtp;

// Feature to section the Content Suggestions into modules.
extern const base::Feature kContentSuggestionsUIModuleRefresh;

// Feature to show the Trending Queries module.
extern const base::Feature kTrendingQueriesModule;

// Feature params for kTrendingQueriesModule.
extern const char kTrendingQueriesHideShortcutsParam[];
extern const char kTrendingQueriesDisabledFeedParam[];
extern const char kTrendingQueriesSignedOutParam[];
extern const char kTrendingQueriesNeverShowModuleParam[];

// A parameter to indicate whether the native UI is enabled for the discover
// feed.
extern const char kDiscoverFeedIsNativeUIEnabled[];

// Whether the Discover feed is enabled instead of the Zine feed.
bool IsDiscoverFeedEnabled();

// Whether the Content Suggestions UI Module Refresh feature is enabled.
bool IsContentSuggestionsUIModuleRefreshEnabled();

// Whether the Trending Queries module feature is enabled.
bool IsTrendingQueriesModuleEnabled();

// Whether the shorctus should be hidden while showing the Trending Queries
// module.
bool ShouldHideShortcutsForTrendingQueries();

// Whether the Trending Queries module should only be shown to users who had the
// feed disabled.
bool ShouldOnlyShowTrendingQueriesForDisabledFeed();

// Whether the Trending Queries module should only be shown to signed out users.
bool ShouldOnlyShowTrendingQueriesForSignedOut();

// Whether the Trending Queries module should not be shown even if the feature
// is enabled.
bool ShouldNeverShowTrendingQueriesModule();

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_FEATURE_H_

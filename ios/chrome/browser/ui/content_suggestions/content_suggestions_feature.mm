// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"

#import "base/metrics/field_trial_params.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Feature disabled by default to keep showing old Zine feed.
const base::Feature kDiscoverFeedInNtp{"DiscoverFeedInNtp",
                                       base::FEATURE_ENABLED_BY_DEFAULT};

// Feature disabled by default.
const base::Feature kSingleNtp{"SingleNTP", base::FEATURE_ENABLED_BY_DEFAULT};

// Feature disabled by default.
const base::Feature kContentSuggestionsUIModuleRefresh{
    "ContentSuggestionsUIModuleRefresh", base::FEATURE_DISABLED_BY_DEFAULT};

// Feature disabled by default.
const base::Feature kTrendingQueriesModule{"TrendingQueriesModule",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

const char kTrendingQueriesHideShortcutsParam[] = "hide_shortcuts";
const char kTrendingQueriesDisabledFeedParam[] = "disabled_feed_only";
const char kTrendingQueriesSignedOutParam[] = "signed_out_only";
const char kTrendingQueriesNeverShowModuleParam[] = "never_show_module";

// A parameter to indicate whether the native UI is enabled for the discover
// feed.
const char kDiscoverFeedIsNativeUIEnabled[] = "DiscoverFeedIsNativeUIEnabled";

bool IsDiscoverFeedEnabled() {
  return base::FeatureList::IsEnabled(kDiscoverFeedInNtp);
}

bool IsContentSuggestionsUIModuleRefreshEnabled() {
  return base::FeatureList::IsEnabled(kContentSuggestionsUIModuleRefresh);
}

bool IsTrendingQueriesModuleEnabled() {
  return base::FeatureList::IsEnabled(kContentSuggestionsUIModuleRefresh) &&
         base::FeatureList::IsEnabled(kTrendingQueriesModule) &&
         !ShouldNeverShowTrendingQueriesModule();
}

bool ShouldHideShortcutsForTrendingQueries() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kTrendingQueriesModule, kTrendingQueriesHideShortcutsParam, false);
}

bool ShouldOnlyShowTrendingQueriesForDisabledFeed() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kTrendingQueriesModule, kTrendingQueriesDisabledFeedParam, false);
}

bool ShouldOnlyShowTrendingQueriesForSignedOut() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kTrendingQueriesModule, kTrendingQueriesSignedOutParam, false);
}

bool ShouldNeverShowTrendingQueriesModule() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kTrendingQueriesModule, kTrendingQueriesNeverShowModuleParam, false);
}

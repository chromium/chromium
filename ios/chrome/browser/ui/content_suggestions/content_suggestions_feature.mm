// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"

#import "base/metrics/field_trial.h"
#import "base/metrics/field_trial_params.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/application_context/application_context.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

const char kTrialPrefName[] = "trending_queries.trial_version";

// Update this if the experiment configuration is fundamentally changing. This
// will mean clients in the study will fall out of the client-side FieldTrial
// and be open to enabling the features based on the server-side config.
// Otherwise, the existing groups should simply be marked (not renamed) as
// postperiod, their params updated to default values, and new groups be created
// for any followup experimentation.
const int kCurrentTrialVersion = 3;

// Feature disabled by default to keep showing old Zine feed.
BASE_FEATURE(kDiscoverFeedInNtp,
             "DiscoverFeedInNtp",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Feature disabled by default.
BASE_FEATURE(kSingleNtp, "SingleNTP", base::FEATURE_ENABLED_BY_DEFAULT);

// Feature disabled by default.
BASE_FEATURE(kContentSuggestionsUIModuleRefresh,
             "ContentSuggestionsUIModuleRefresh",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kContentSuggestionsUIModuleRefreshNewUser,
             "ContentSuggestionsUIModuleRefreshNewUser",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kContentSuggestionsUIModuleRefreshFlagOverrideFieldTrialName[] =
    "ContentSuggestionsUIModuleRefreshFlagOverrideFieldTria";

const char kContentSuggestionsUIModuleRefreshMinimizeSpacingParam[] =
    "minimize_spacing";
const char kContentSuggestionsUIModuleRefreshRemoveHeadersParam[] =
    "remove_headers";

BASE_FEATURE(kHideMVTAndShortcutsForNewUsers,
             "HideMostVisitedAndShortcutsForNewUsers",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Feature disabled by default.
BASE_FEATURE(kTrendingQueriesModule,
             "TrendingQueriesModule",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTrendingQueriesModuleNewUser,
             "TrendingQueriesModuleNewUser",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kTrendingQueriesHideShortcutsParam[] = "hide_shortcuts";
const char kTrendingQueriesDisabledFeedParam[] = "disabled_feed_only";

const char kModularHomeTrendingQueriesClientSideFieldTrialName[] =
    "ModularHomeTrendingQueriesNewUsers";

const char kTrendingQueriesFlagOverrideFieldTrialName[] =
    "TrendingQueriesFlagOverride";

// A parameter to indicate whether the native UI is enabled for the discover
// feed.
const char kDiscoverFeedIsNativeUIEnabled[] = "DiscoverFeedIsNativeUIEnabled";

bool IsDiscoverFeedEnabled() {
  return base::FeatureList::IsEnabled(kDiscoverFeedInNtp);
}

bool IsHideMVTAndShortcutsEnabled() {
  return base::FeatureList::IsEnabled(kHideMVTAndShortcutsForNewUsers);
}

// Returns true if the client is bucketed into an enabled experiment group of
// the current active FieldTrial of
// kModularHomeTrendingQueriesClientSideFieldTrialName.
// This allows for the code to capture the state of the FieldTrial for new
// clients that should be sticky and take precedence over the parallel studies
// that may be coming from the server-side config meant for existing clients,
// who then won't report being in those studies. If kCurrentTrialVersion gets
// updates and a client has a previous version, then it will then be open to
// enabling features based on the server-side config.
bool IsClientInCurrentModularHomeTrendingQueryStudy() {
  return GetApplicationContext()->GetLocalState()->GetInteger(kTrialPrefName) ==
         kCurrentTrialVersion;
}

bool IsContentSuggestionsUIModuleRefreshEnabled() {
  // If the feature has been overriden in chrome://flags, ensure the correct
  // feature is checked (e.g. not the one used for the client-side FieldTrial).
  if (base::FieldTrialList::TrialExists(
          kContentSuggestionsUIModuleRefreshFlagOverrideFieldTrialName)) {
    return base::FeatureList::IsEnabled(kContentSuggestionsUIModuleRefresh);
  }
  if (IsClientInCurrentModularHomeTrendingQueryStudy()) {
    return base::FeatureList::IsEnabled(
        kContentSuggestionsUIModuleRefreshNewUser);
  }
  return base::FeatureList::IsEnabled(kContentSuggestionsUIModuleRefresh);
}

bool IsTrendingQueriesModuleEnabled() {
  // If the feature has been overriden in chrome://flags, ensure the correct
  // feature is checked (e.g. not the one used for the client-side FieldTrial).
  if (base::FieldTrialList::TrialExists(
          kTrendingQueriesFlagOverrideFieldTrialName)) {
    return base::FeatureList::IsEnabled(kContentSuggestionsUIModuleRefresh) &&
           base::FeatureList::IsEnabled(kTrendingQueriesModule);
  }
  // If client is registered in the client-side FieldTrial, check its unique
  // base::Feature for enabled state.
  if (IsClientInCurrentModularHomeTrendingQueryStudy()) {
    return base::FeatureList::IsEnabled(
               kContentSuggestionsUIModuleRefreshNewUser) &&
           base::FeatureList::IsEnabled(kTrendingQueriesModuleNewUser);
  }
  return base::FeatureList::IsEnabled(kContentSuggestionsUIModuleRefresh) &&
         base::FeatureList::IsEnabled(kTrendingQueriesModule);
}

bool ShouldMinimizeSpacingForModuleRefresh() {
  // If the feature has been overriden in chrome://flags, ensure the correct
  // feature is checked (e.g. not the one used for the client-side FieldTrial).
  if (base::FieldTrialList::TrialExists(
          kContentSuggestionsUIModuleRefreshFlagOverrideFieldTrialName)) {
    return base::GetFieldTrialParamByFeatureAsBool(
        kContentSuggestionsUIModuleRefresh,
        kContentSuggestionsUIModuleRefreshMinimizeSpacingParam, false);
  }
  // If client is registered in the client-side FieldTrial, check its unique
  // base::Feature for enabled state.
  if (IsClientInCurrentModularHomeTrendingQueryStudy()) {
    return base::GetFieldTrialParamByFeatureAsBool(
        kContentSuggestionsUIModuleRefreshNewUser,
        kContentSuggestionsUIModuleRefreshMinimizeSpacingParam, false);
  }
  return base::GetFieldTrialParamByFeatureAsBool(
      kContentSuggestionsUIModuleRefresh,
      kContentSuggestionsUIModuleRefreshMinimizeSpacingParam, false);
}

bool ShouldRemoveHeadersForModuleRefresh() {
  // If the feature has been overriden in chrome://flags, ensure the correct
  // feature is checked (e.g. not the one used for the client-side FieldTrial).
  if (base::FieldTrialList::TrialExists(
          kContentSuggestionsUIModuleRefreshFlagOverrideFieldTrialName)) {
    return base::GetFieldTrialParamByFeatureAsBool(
        kContentSuggestionsUIModuleRefresh,
        kContentSuggestionsUIModuleRefreshRemoveHeadersParam, false);
  }
  // If client is registered in the client-side FieldTrial, check its unique
  // base::Feature for enabled state.
  if (IsClientInCurrentModularHomeTrendingQueryStudy()) {
    return base::GetFieldTrialParamByFeatureAsBool(
        kContentSuggestionsUIModuleRefreshNewUser,
        kContentSuggestionsUIModuleRefreshRemoveHeadersParam, false);
  }
  return base::GetFieldTrialParamByFeatureAsBool(
      kContentSuggestionsUIModuleRefresh,
      kContentSuggestionsUIModuleRefreshRemoveHeadersParam, false);
}

bool ShouldHideShortcutsForTrendingQueries() {
  // If the feature has been overriden in chrome://flags, ensure the correct
  // feature is checked (e.g. not the one used for the client-side FieldTrial).
  if (base::FieldTrialList::TrialExists(
          kTrendingQueriesFlagOverrideFieldTrialName)) {
    return base::GetFieldTrialParamByFeatureAsBool(
        kTrendingQueriesModule, kTrendingQueriesHideShortcutsParam, false);
  }
  // If client is registered in the client-side FieldTrial, check its unique
  // base::Feature for enabled state.
  if (IsClientInCurrentModularHomeTrendingQueryStudy()) {
    return base::GetFieldTrialParamByFeatureAsBool(
        kTrendingQueriesModuleNewUser, kTrendingQueriesHideShortcutsParam,
        false);
  }
  return base::GetFieldTrialParamByFeatureAsBool(
      kTrendingQueriesModule, kTrendingQueriesHideShortcutsParam, false);
}

bool ShouldOnlyShowTrendingQueriesForDisabledFeed() {
  // This param is not used in the client-side FieldTrial, so no need to make
  // checks since chrome://flags and server-side config use same Feature name.
  return base::GetFieldTrialParamByFeatureAsBool(
      kTrendingQueriesModule, kTrendingQueriesDisabledFeedParam, false);
}

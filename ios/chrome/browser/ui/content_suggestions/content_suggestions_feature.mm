// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"

#import "base/metrics/field_trial.h"
#import "base/metrics/field_trial_params.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"

// Feature disabled by default to keep showing old Zine feed.
BASE_FEATURE(kDiscoverFeedInNtp,
             "DiscoverFeedInNtp",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Feature disabled by default.
BASE_FEATURE(kSingleNtp, "SingleNTP", base::FEATURE_ENABLED_BY_DEFAULT);

// Feature disabled by default.
BASE_FEATURE(kMagicStack, "MagicStack", base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kHideContentSuggestionsTiles,
             "HideContentSuggestionsTiles",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabResumption,
             "TabResumption",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kMagicStackMostVisitedModuleParam[] = "MagicStackMostVisitedModule";

const char kReducedSpaceParam[] = "ReducedNTPTopSpace";

// A parameter to indicate whether the native UI is enabled for the discover
// feed.
const char kDiscoverFeedIsNativeUIEnabled[] = "DiscoverFeedIsNativeUIEnabled";

const char kHideContentSuggestionsTilesParamMostVisited[] = "HideMostVisited";
const char kHideContentSuggestionsTilesParamShortcuts[] = "HideShortcuts";

bool IsDiscoverFeedEnabled() {
  return base::FeatureList::IsEnabled(kDiscoverFeedInNtp);
}

bool IsMagicStackEnabled() {
  return base::FeatureList::IsEnabled(kMagicStack);
}

bool ShouldPutMostVisitedSitesInMagicStack() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kMagicStack, kMagicStackMostVisitedModuleParam, false);
}

double ReducedNTPTopMarginSpaceForMagicStack() {
  return base::GetFieldTrialParamByFeatureAsDouble(kMagicStack,
                                                   kReducedSpaceParam, 0);
}

bool ShouldHideMVT() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kHideContentSuggestionsTiles,
      kHideContentSuggestionsTilesParamMostVisited, false);
}

bool ShouldHideShortcuts() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kHideContentSuggestionsTiles, kHideContentSuggestionsTilesParamShortcuts,
      false);
}

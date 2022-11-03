// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/new_tab_page_feature.h"

#import "base/ios/ios_util.h"
#import "base/metrics/field_trial_params.h"
#import "ios/chrome/browser/prefs/pref_names.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BASE_FEATURE(kEnableDiscoverFeedPreview,
             "EnableDiscoverFeedPreview",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDiscoverFeedGhostCardsEnabled,
             "DiscoverFeedGhostCardsEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableDiscoverFeedDiscoFeedEndpoint,
             "EnableDiscoFeedEndpoint",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableDiscoverFeedStaticResourceServing,
             "EnableDiscoverFeedStaticResourceServing",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableDiscoverFeedTopSyncPromo,
             "EnableDiscoverFeedTopSyncPromo",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFollowingFeedDefaultSortType,
             "FollowingFeedDefaultSortType",
             base::FEATURE_DISABLED_BY_DEFAULT);

// A parameter value for the number of impressions before autodismissing the
// promo.
const char kDiscoverFeedTopSyncPromoAutodismissImpressions[] =
    "autodismissImpressions";

const char kDiscoverFeedSRSReconstructedTemplatesEnabled[] =
    "DiscoverFeedSRSReconstructedTemplatesEnabled";

const char kDiscoverFeedSRSPreloadTemplatesEnabled[] =
    "DiscoverFeedSRSPreloadTemplatesEnabled";

BASE_FEATURE(kNTPViewHierarchyRepair,
             "NTPViewHierarchyRepair",
             base::FEATURE_ENABLED_BY_DEFAULT);

const char kDiscoverFeedTopSyncPromoStyleFullWithTitle[] = "fullWithTitle";
const char kDiscoverFeedTopSyncPromoStyleCompact[] = "compact";

const char kFollowingFeedDefaultSortTypeSortByLatest[] = "SortByLatest";
const char kFollowingFeedDefaultSortTypeGroupedByPublisher[] =
    "GroupedByPublisher";

BASE_FEATURE(kEnableFeedAblation,
             "FeedAblationEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableCheckVisibilityOnAttentionLogStart,
             "EnableCheckVisibilityOnAttentionLogStart",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableRefineDataSourceReloadReporting,
             "EnableRefineDataSourceReloadReporting",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsDiscoverFeedPreviewEnabled() {
  return base::FeatureList::IsEnabled(kEnableDiscoverFeedPreview);
}

bool IsDiscoverFeedGhostCardsEnabled() {
  return base::FeatureList::IsEnabled(kDiscoverFeedGhostCardsEnabled);
}

bool IsNTPViewHierarchyRepairEnabled() {
  return base::FeatureList::IsEnabled(kNTPViewHierarchyRepair);
}

bool IsDiscoverFeedTopSyncPromoEnabled() {
  return base::FeatureList::IsEnabled(kEnableDiscoverFeedTopSyncPromo);
}

bool IsDiscoverFeedTopSyncPromoCompact() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kEnableDiscoverFeedTopSyncPromo, kDiscoverFeedTopSyncPromoStyleCompact,
      false);
}

int FeedSyncPromoAutodismissCount() {
  return base::GetFieldTrialParamByFeatureAsInt(
      kEnableDiscoverFeedTopSyncPromo,
      kDiscoverFeedTopSyncPromoAutodismissImpressions, 10);
}

bool IsFollowingFeedDefaultSortTypeEnabled() {
  return base::FeatureList::IsEnabled(kFollowingFeedDefaultSortType);
}

bool IsDefaultFollowingFeedSortTypeGroupedByPublisher() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kFollowingFeedDefaultSortType,
      kFollowingFeedDefaultSortTypeGroupedByPublisher, true);
}

bool IsFeedAblationEnabled() {
  return base::FeatureList::IsEnabled(kEnableFeedAblation);
}

bool IsContentSuggestionsForSupervisedUserEnabled(PrefService* pref_service) {
  return pref_service->GetBoolean(
      prefs::kNTPContentSuggestionsForSupervisedUserEnabled);
}

bool IsCheckVisibilityOnAttentionLogStartEnabled() {
  return base::FeatureList::IsEnabled(
      kEnableCheckVisibilityOnAttentionLogStart);
}

bool IsRefineDataSourceReloadReportingEnabled() {
  return base::FeatureList::IsEnabled(kEnableRefineDataSourceReloadReporting);
}

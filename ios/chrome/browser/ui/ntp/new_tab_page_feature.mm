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

#pragma mark - Feature declarations

BASE_FEATURE(kEnableDiscoverFeedPreview,
             "EnableDiscoverFeedPreview",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableDiscoverFeedStaticResourceServing,
             "EnableDiscoverFeedStaticResourceServing",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableDiscoverFeedDiscoFeedEndpoint,
             "EnableDiscoFeedEndpoint",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableDiscoverFeedTopSyncPromo,
             "EnableDiscoverFeedTopSyncPromo",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableFollowingFeedDefaultSortType,
             "EnableFollowingFeedDefaultSortType",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableNTPViewHierarchyRepair,
             "NTPViewHierarchyRepair",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableFeedAblation,
             "EnableFeedAblation",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableCheckVisibilityOnAttentionLogStart,
             "EnableCheckVisibilityOnAttentionLogStart",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableRefineDataSourceReloadReporting,
             "EnableRefineDataSourceReloadReporting",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Flag to override feed settings through the server. Enabling this feature on
// its own does nothing; relies on feature parameters.
BASE_FEATURE(kOverrideFeedSettings,
             "OverrideFeedSettings",
             base::FEATURE_DISABLED_BY_DEFAULT);

#pragma mark - Feature parameters

const char kDiscoverFeedSRSReconstructedTemplatesEnabled[] =
    "DiscoverFeedSRSReconstructedTemplatesEnabled";

const char kDiscoverFeedSRSPreloadTemplatesEnabled[] =
    "DiscoverFeedSRSPreloadTemplatesEnabled";

// EnableDiscoverFeedTopSyncPromo parameters.
const char kDiscoverFeedTopSyncPromoAutodismissImpressions[] =
    "autodismissImpressions";
const char kDiscoverFeedTopSyncPromoStyleFullWithTitle[] = "fullWithTitle";
const char kDiscoverFeedTopSyncPromoStyleCompact[] = "compact";

// EnableFollowingFeedDefaultSortType parameters.
const char kFollowingFeedDefaultSortTypeSortByLatest[] = "SortByLatest";
const char kFollowingFeedDefaultSortTypeGroupedByPublisher[] =
    "GroupedByPublisher";

// Feature parameters for `kOverrideFeedSettings`.
const char kFeedSettingRefreshThresholdInSeconds[] =
    "RefreshThresholdInSeconds";
const char kFeedSettingMaximumDataCacheAgeInSeconds[] =
    "MaximumDataCacheAgeInSeconds";
const char kFeedSettingTimeoutThresholdAfterClearBrowsingData[] =
    "TimeoutThresholdAfterClearBrowsingData";
const char kFeedSettingDiscoverReferrerParameter[] =
    "DiscoverReferrerParameter";

#pragma mark - Helpers

bool IsDiscoverFeedPreviewEnabled() {
  return base::FeatureList::IsEnabled(kEnableDiscoverFeedPreview);
}

bool IsNTPViewHierarchyRepairEnabled() {
  return base::FeatureList::IsEnabled(kEnableNTPViewHierarchyRepair);
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
  return base::FeatureList::IsEnabled(kEnableFollowingFeedDefaultSortType);
}

bool IsDefaultFollowingFeedSortTypeGroupedByPublisher() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kEnableFollowingFeedDefaultSortType,
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

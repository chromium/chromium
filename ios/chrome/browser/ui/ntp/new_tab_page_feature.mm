// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/new_tab_page_feature.h"

#import "base/ios/ios_util.h"
#import "base/metrics/field_trial_params.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
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

BASE_FEATURE(kEnableCheckVisibilityOnAttentionLogStart,
             "EnableCheckVisibilityOnAttentionLogStart",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableRefineDataSourceReloadReporting,
             "EnableRefineDataSourceReloadReporting",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kFeedHeaderSettings,
             "FeedHeaderSettings",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kOverrideFeedSettings,
             "OverrideFeedSettings",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableFeedSyntheticCapabilities,
             "EnableFeedSyntheticCapabilities",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kWebFeedFeedbackReroute,
             "WebFeedFeedbackReroute",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableFollowManagementInstantReload,
             "EnableFollowManagementInstantReload",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableSignedOutViewDemotion,
             "EnableSignedOutViewDemotion",
             base::FEATURE_DISABLED_BY_DEFAULT);

#pragma mark - Feature parameters

const char kDiscoverFeedSRSReconstructedTemplatesEnabled[] =
    "DiscoverFeedSRSReconstructedTemplatesEnabled";

const char kDiscoverFeedSRSPreloadTemplatesEnabled[] =
    "DiscoverFeedSRSPreloadTemplatesEnabled";

// EnableDiscoverFeedTopSyncPromo parameters.
const char kDiscoverFeedTopSyncPromoStyle[] = "DiscoverFeedTopSyncPromoStyle";
const char kDiscoverFeedTopSyncPromoAutodismissImpressions[] =
    "autodismissImpressions";
const char kDiscoverFeedTopSyncPromoIgnoreEngagementCondition[] =
    "IgnoreFeedEngagementConditionForTopSyncPromo";

// EnableFollowingFeedDefaultSortType parameters.
const char kFollowingFeedDefaultSortTypeSortByLatest[] = "SortByLatest";
const char kFollowingFeedDefaultSortTypeGroupedByPublisher[] =
    "GroupedByPublisher";

// Feature parameters for `kFeedHeaderSettings`.
const char kEnableDotForNewFollowedContent[] =
    "kEnableDotForNewFollowedContent";
const char kDisableStickyHeaderForFollowingFeed[] =
    "DisableStickyHeaderForFollowingFeed";
const char kOverrideFeedHeaderHeight[] = "OverrideFeedHeaderHeight";

// Feature parameters for `kOverrideFeedSettings`.
const char kFeedSettingRefreshThresholdInSeconds[] =
    "RefreshThresholdInSeconds";
const char kFeedSettingUnseenRefreshThresholdInSeconds[] =
    "UnseenRefreshThresholdInSeconds";
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

SigninPromoViewStyle GetTopOfFeedPromoStyle() {
  CHECK(IsDiscoverFeedTopSyncPromoEnabled());
  // Defaults to Compact Titled (Unpersonalized).
  return (SigninPromoViewStyle)base::GetFieldTrialParamByFeatureAsInt(
      kEnableDiscoverFeedTopSyncPromo, kDiscoverFeedTopSyncPromoStyle, 1);
}

bool ShouldIgnoreFeedEngagementConditionForTopSyncPromo() {
  CHECK(IsDiscoverFeedTopSyncPromoEnabled());
  return base::GetFieldTrialParamByFeatureAsBool(
      kEnableDiscoverFeedTopSyncPromo,
      kDiscoverFeedTopSyncPromoIgnoreEngagementCondition, false);
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

bool IsStickyHeaderDisabledForFollowingFeed() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kFeedHeaderSettings, kDisableStickyHeaderForFollowingFeed, true);
}

bool IsDotEnabledForNewFollowedContent() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kFeedHeaderSettings, kEnableDotForNewFollowedContent, false);
}

bool IsFeedSyntheticCapabilitiesEnabled() {
  return base::FeatureList::IsEnabled(kEnableFeedSyntheticCapabilities);
}

int FollowingFeedHeaderHeight() {
  int defaultWebChannelsHeaderHeight = 30;
  return base::GetFieldTrialParamByFeatureAsInt(kFeedHeaderSettings,
                                                kOverrideFeedHeaderHeight,
                                                defaultWebChannelsHeaderHeight);
}

bool IsWebFeedFeedbackRerouteEnabled() {
  return base::FeatureList::IsEnabled(kWebFeedFeedbackReroute);
}

bool IsFollowManagementInstantReloadEnabled() {
  return base::FeatureList::IsEnabled(kEnableFollowManagementInstantReload);
}

bool IsSignedOutViewDemotionEnabled() {
  return base::FeatureList::IsEnabled(kEnableSignedOutViewDemotion);
}

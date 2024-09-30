// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"

#import "base/ios/ios_util.h"
#import "base/metrics/field_trial_params.h"
#import "components/variations/service/variations_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"

#pragma mark - Constants

#pragma mark - Feature declarations

BASE_FEATURE(kEnableDiscoverFeedStaticResourceServing,
             "EnableDiscoverFeedStaticResourceServing",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableDiscoverFeedDiscoFeedEndpoint,
             "EnableDiscoFeedEndpoint",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableNTPViewHierarchyRepair,
             "NTPViewHierarchyRepair",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kOverrideFeedSettings,
             "OverrideFeedSettings",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kWebFeedFeedbackReroute,
             "WebFeedFeedbackReroute",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableSignedOutViewDemotion,
             "EnableSignedOutViewDemotion",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableiPadFeedGhostCards,
             "EnableiPadFeedGhostCards",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIdentityDiscAccountMenu,
             "IdentityDiscAccountMenu",
             base::FEATURE_DISABLED_BY_DEFAULT);

#pragma mark - Feature parameters

const char kDiscoverFeedSRSReconstructedTemplatesEnabled[] =
    "DiscoverFeedSRSReconstructedTemplatesEnabled";

const char kDiscoverFeedSRSPreloadTemplatesEnabled[] =
    "DiscoverFeedSRSPreloadTemplatesEnabled";

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

bool IsNTPViewHierarchyRepairEnabled() {
  return base::FeatureList::IsEnabled(kEnableNTPViewHierarchyRepair);
}

bool IsDiscoverFeedTopSyncPromoEnabled() {
  // Promo should not be shown on FRE, or for users in Great Britain for AADC
  // compliance.
  variations::VariationsService* variations_service =
      GetApplicationContext()->GetVariationsService();
  return variations_service &&
         variations_service->GetStoredPermanentCountry() != "gb";
}

bool IsContentSuggestionsForSupervisedUserEnabled(PrefService* pref_service) {
  return pref_service->GetBoolean(
      prefs::kNTPContentSuggestionsForSupervisedUserEnabled);
}

bool IsWebFeedFeedbackRerouteEnabled() {
  return base::FeatureList::IsEnabled(kWebFeedFeedbackReroute);
}

bool IsSignedOutViewDemotionEnabled() {
  return base::FeatureList::IsEnabled(kEnableSignedOutViewDemotion);
}

bool IsiPadFeedGhostCardsEnabled() {
  return base::FeatureList::IsEnabled(kEnableiPadFeedGhostCards);
}

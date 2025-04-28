// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"

#import "base/ios/ios_util.h"
#import "base/metrics/field_trial_params.h"
#import "components/prefs/pref_service.h"
#import "components/variations/service/variations_service.h"
#import "ios/chrome/browser/authentication/ui_bundled/cells/signin_promo_view_constants.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"

#pragma mark - Constants

const char kDeprecateFeedHeaderParameterRemoveLabel[] = "remove-feed-label";
const char kDeprecateFeedHeaderParameterEnlargeLogoAndFakebox[] =
    "enlarge-logo-n-fakebox";
const char kDeprecateFeedHeaderParameterTopPadding[] = "top-padding";
const char kDeprecateFeedHeaderParameterSearchFieldTopMargin[] =
    "search-field-top-margin";
const char kDeprecateFeedHeaderParameterSpaceBetweenModules[] =
    "space-between-modules";
const char kDeprecateFeedHeaderParameterHeaderBottomPadding[] =
    "header-bottom-padding";

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

BASE_FEATURE(kFeedSwipeInProductHelp,
             "FeedSwipeInProductHelp",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kUseFeedEligibilityService,
             "UseFeedEligibilityService",
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

// Feature parameter for `kIdentityDiscAccountMenu`.
const char kShowSettingsInAccountMenuParam[] =
    "identity-disc-account-menu-with-settings-button";

const char kFeedSwipeInProductHelpArmParam[] = "feed-swipe-in-product-help-arm";

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

bool ShouldRemoveDiscoverLabel(bool is_google_default_search_engine) {
  return is_google_default_search_engine && ShouldDeprecateFeedHeader() &&
         base::GetFieldTrialParamByFeatureAsBool(
             kDeprecateFeedHeader, kDeprecateFeedHeaderParameterRemoveLabel,
             false);
}

bool ShouldEnlargeLogoAndFakebox() {
  return ShouldDeprecateFeedHeader() &&
         base::GetFieldTrialParamByFeatureAsBool(
             kDeprecateFeedHeader,
             kDeprecateFeedHeaderParameterEnlargeLogoAndFakebox, false);
}

double TopPaddingToNTP() {
  return ShouldDeprecateFeedHeader()
             ? base::GetFieldTrialParamByFeatureAsDouble(
                   kDeprecateFeedHeader,
                   kDeprecateFeedHeaderParameterTopPadding, 0)
             : 0;
}

double GetDeprecateFeedHeaderParameterValueAsDouble(
    const std::string& param_name,
    double default_value) {
  if (!ShouldDeprecateFeedHeader()) {
    return default_value;
  }
  return base::GetFieldTrialParamByFeatureAsDouble(kDeprecateFeedHeader,
                                                   param_name, default_value);
}

bool IdentityDiscAccountMenuEnabledWithSettings() {
  if (base::FeatureList::IsEnabled(kIdentityDiscAccountMenu)) {
    return base::GetFieldTrialParamByFeatureAsBool(
        kIdentityDiscAccountMenu, kShowSettingsInAccountMenuParam, false);
  }
  return false;
}

FeedSwipeIPHVariation GetFeedSwipeIPHVariation() {
  if (base::FeatureList::IsEnabled(kFeedSwipeInProductHelp)) {
    return static_cast<FeedSwipeIPHVariation>(
        base::GetFieldTrialParamByFeatureAsInt(
            kFeedSwipeInProductHelp,
            kFeedSwipeInProductHelpArmParam, /*default_value=*/
            static_cast<int>(FeedSwipeIPHVariation::kStaticAfterFRE)));
  }
  return FeedSwipeIPHVariation::kDisabled;
}

bool UseFeedEligibilityService() {
  return base::FeatureList::IsEnabled(kUseFeedEligibilityService);
}

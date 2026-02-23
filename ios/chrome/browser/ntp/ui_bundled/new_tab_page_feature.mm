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
#import "ui/base/device_form_factor.h"

#pragma mark - Feature declarations

BASE_FEATURE(kEnableNTPViewHierarchyRepair,
             "NTPViewHierarchyRepair",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kOverrideFeedSettings, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFeedSwipeInProductHelp, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kUseFeedEligibilityService, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kMostVisitedTilesCustomizationIOS,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableNTPBackgroundImageCache, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kConsistentLogoDoodleHeight, base::FEATURE_DISABLED_BY_DEFAULT);

#pragma mark - Feature parameters

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

const char kFeedSwipeInProductHelpArmParam[] = "feed-swipe-in-product-help-arm";

BASE_FEATURE_PARAM(int,
                   kFeedSwipeInProductHelpArmParamFeature,
                   &kFeedSwipeInProductHelp,
                   kFeedSwipeInProductHelpArmParam,
                   static_cast<int>(FeedSwipeIPHVariation::kStaticAfterFRE));

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

FeedSwipeIPHVariation GetFeedSwipeIPHVariation() {
  if (base::FeatureList::IsEnabled(kFeedSwipeInProductHelp)) {
    return static_cast<FeedSwipeIPHVariation>(
        kFeedSwipeInProductHelpArmParamFeature.Get());
  }
  return FeedSwipeIPHVariation::kDisabled;
}

bool UseFeedEligibilityService() {
  return base::FeatureList::IsEnabled(kUseFeedEligibilityService);
}

NTPMIAEntrypointVariation GetNTPMIAEntrypointVariation() {
  std::string feature_param = base::GetFieldTrialParamValueByFeature(
      kNTPMIAEntrypoint, kNTPMIAEntrypointParam);
  if (feature_param == kNTPMIAEntrypointParamOmniboxContainedSingleButton) {
    return NTPMIAEntrypointVariation::kOmniboxContainedSingleButton;
  } else if (feature_param == kNTPMIAEntrypointParamOmniboxContainedInline) {
    return NTPMIAEntrypointVariation::kOmniboxContainedInline;
  } else if (feature_param ==
             kNTPMIAEntrypointParamOmniboxContainedEnlargedFakebox) {
    return NTPMIAEntrypointVariation::kOmniboxContainedEnlargedFakebox;
  } else if (feature_param ==
             kNTPMIAEntrypointParamEnlargedFakeboxNoIncognito) {
    return NTPMIAEntrypointVariation::kEnlargedFakeboxNoIncognito;
  } else if (feature_param == kNTPMIAEntrypointParamAIMInQuickActions) {
    return NTPMIAEntrypointVariation::kAIMInQuickAction;
  } else {
    // Disabled on iPad.
    if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET &&
        !base::FeatureList::IsEnabled(kAIMNTPEntrypointTablet)) {
      return NTPMIAEntrypointVariation::kDisabled;
    }
    // Default value.
    return NTPMIAEntrypointVariation::kAIMInQuickAction;
  }
}

bool ShowOnlyMIAEntrypointInNTPFakebox() {
  NTPMIAEntrypointVariation variation = GetNTPMIAEntrypointVariation();
  return variation ==
             NTPMIAEntrypointVariation::kOmniboxContainedSingleButton ||
         variation ==
             NTPMIAEntrypointVariation::kOmniboxContainedEnlargedFakebox ||
         variation == NTPMIAEntrypointVariation::kEnlargedFakeboxNoIncognito;
}

bool ShouldShowQuickActionsRow() {
  NTPMIAEntrypointVariation variation = GetNTPMIAEntrypointVariation();
  return ShowOnlyMIAEntrypointInNTPFakebox() ||
         variation == NTPMIAEntrypointVariation::kAIMInQuickAction;
}

bool ShouldEnlargeNTPFakeboxForMIA() {
  NTPMIAEntrypointVariation variation = GetNTPMIAEntrypointVariation();
  return variation ==
             NTPMIAEntrypointVariation::kOmniboxContainedEnlargedFakebox ||
         variation == NTPMIAEntrypointVariation::kEnlargedFakeboxNoIncognito ||
         variation == NTPMIAEntrypointVariation::kAIMInQuickAction;
}

bool IsContentSuggestionsCustomizable() {
  return base::FeatureList::IsEnabled(kMostVisitedTilesCustomizationIOS);
}

bool IsNTPBackgroundImageCacheEnabled() {
  return base::FeatureList::IsEnabled(kEnableNTPBackgroundImageCache);
}

bool IsConsistentLogoDoodleHeightEnabled() {
  return base::FeatureList::IsEnabled(kConsistentLogoDoodleHeight);
}

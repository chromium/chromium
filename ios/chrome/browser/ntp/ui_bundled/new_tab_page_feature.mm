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

BASE_FEATURE(kOverrideFeedSettings, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kNTPHeaderUseTransformsForAnimations,
             base::FEATURE_DISABLED_BY_DEFAULT);

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

bool IsAimEnabledInNtp() {
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET &&
      !base::FeatureList::IsEnabled(kAIMNTPEntrypointTablet)) {
    return NO;
  }

  return YES;
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

bool IsNTPHeaderTransformsForAnimationsEnabled() {
  return base::FeatureList::IsEnabled(kNTPHeaderUseTransformsForAnimations);
}

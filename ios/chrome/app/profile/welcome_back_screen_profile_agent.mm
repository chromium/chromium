// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/profile/welcome_back_screen_profile_agent.h"

#import <algorithm>

#import "base/check.h"
#import "base/metrics/histogram_functions.h"
#import "base/time/time.h"
#import "components/password_manager/core/browser/password_manager_util.h"
#import "components/prefs/pref_service.h"
#import "components/previous_session_info/previous_session_info.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/first_run/public/best_features_item.h"
#import "ios/chrome/browser/first_run/public/features.h"
#import "ios/chrome/browser/metrics/model/ios_profile_session_durations_service.h"
#import "ios/chrome/browser/metrics/model/ios_profile_session_durations_service_factory.h"
#import "ios/chrome/browser/promos_manager/model/constants.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/welcome_back/metrics/welcome_back_metrics.h"
#import "ios/chrome/browser/welcome_back/model/features.h"
#import "ios/chrome/browser/welcome_back/model/welcome_back_prefs.h"

namespace {
// Minimum number of features required to show the promo.
constexpr size_t kMinEligibleFeatures = 2;

// Histogram names.
const char kWelcomeBackDaysSinceActiveHistogram[] =
    "IOS.WelcomeBack.DaysSinceActive";
const char kWelcomeBackDaysSinceActiveNotFeatureFlagGuardedHistogram[] =
    "IOS.WelcomeBack.DaysSinceActiveNotFeatureFlagGuarded";
const char kWelcomeBackPromoRegistrationResultHistogram[] =
    "IOS.WelcomeBack.PromoRegistrationResult";
const char kWelcomeBackPromoRegistrationMissingFeatureHistogram[] =
    "IOS.WelcomeBack.PromoRegistration.MissingFeature";
}  // namespace

@implementation WelcomeBackScreenProfileAgent

#pragma mark - ProfileStateObserver

- (void)profileState:(ProfileState*)profileState
    didTransitionToInitStage:(ProfileInitStage)nextInitStage
               fromInitStage:(ProfileInitStage)fromInitStage {
  if (nextInitStage != ProfileInitStage::kFinal) {
    return;
  }

  DCHECK(profileState.profile);

  [self recordDaysSinceActiveHistogramWithName:
            kWelcomeBackDaysSinceActiveNotFeatureFlagGuardedHistogram];

  switch (GetWelcomeBackScreenVariationType()) {
    case WelcomeBackScreenVariationType::kDisabled:
      break;
    case WelcomeBackScreenVariationType::kBasicsWithLockedIncognitoTabs:
    case WelcomeBackScreenVariationType::kBasicsWithPasswords:
    case WelcomeBackScreenVariationType::kProductivityAndShopping:
      [self maybeRegisterPromo];
      break;
    case WelcomeBackScreenVariationType::kSignInBenefits:
      signin::IdentityManager* identityManager =
          IdentityManagerFactory::GetForProfile(profileState.profile);
      if (identityManager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
        [self maybeRegisterPromo];
      }
      break;
  }

  [profileState removeObserver:self];
  [profileState removeAgent:self];
}

#pragma mark - Private

// Determine if the Welcome Back promo should be registered. Register the
// Welcome Back Promo if all criteria is met. If not, deregister the Welcome
// Back promo.
- (void)maybeRegisterPromo {
  [self recordDaysSinceActiveHistogramWithName:
            kWelcomeBackDaysSinceActiveHistogram];

  // Mark Autofill feature as used if the Credential Provider Extension is
  // enabled on startup.
  PrefService* localState = GetApplicationContext()->GetLocalState();
  if (password_manager_util::IsCredentialProviderEnabledOnStartup(localState)) {
    MarkWelcomeBackFeatureUsed(
        BestFeaturesItemType::kAutofillPasswordsInOtherApps);
  }

  // Only log metrics and evaluate registration if Welcome Back is enabled and
  // the Best Features First Run screen is disabled.
  if (!IsWelcomeBackEnabled() ||
      base::FeatureList::IsEnabled(first_run::kBestFeaturesScreenInFirstRun)) {
    return;
  }

  NSDate* lastSessionEndTime =
      [PreviousSessionInfo sharedInstance].sessionEndTime;
  base::TimeDelta timeSinceActive =
      [self timeSinceActiveWithLastSessionEndTime:lastSessionEndTime];

  WelcomeBackPromoRegistrationResult result =
      [self promoRegistrationResultWithLastSessionEndTime:lastSessionEndTime
                                          timeSinceActive:timeSinceActive];

  base::UmaHistogramEnumeration(kWelcomeBackPromoRegistrationResultHistogram,
                                result);

  switch (result) {
    case WelcomeBackPromoRegistrationResult::kSuccess:
      PromosManagerFactory::GetForProfile(self.profileState.profile)
          ->RegisterPromoForSingleDisplay(promos_manager::Promo::WelcomeBack);
      break;
    case WelcomeBackPromoRegistrationResult::kFailureMinEligibleFeaturesNotMet:
      [self logMissingWelcomeBackFeatures];
      break;
    case WelcomeBackPromoRegistrationResult::kFailureTimeSinceActiveLimitNotMet:
    case WelcomeBackPromoRegistrationResult::kFailureSessionEndTimeNil:
      break;
  }
}

// Calculates the elapsed days since the last active session and records it to
// the specified histogram.
- (void)recordDaysSinceActiveHistogramWithName:(const char*)histogramName {
  NSDate* lastSessionEndTime =
      [PreviousSessionInfo sharedInstance].sessionEndTime;
  base::TimeDelta timeSinceActive =
      [self timeSinceActiveWithLastSessionEndTime:lastSessionEndTime];

  if (lastSessionEndTime) {
    int daysSinceActive = timeSinceActive.InDays();
    base::UmaHistogramExactLinear(histogramName, daysSinceActive, 49);
  }
}

// Returns the time elapsed since the last active session.
- (base::TimeDelta)timeSinceActiveWithLastSessionEndTime:
    (NSDate*)lastSessionEndTime {
  return lastSessionEndTime
             ? base::Time::Now() - base::Time::FromNSDate(lastSessionEndTime)
             : base::TimeDelta();
}

// Evaluates registration conditions and returns the resulting outcome of the
// check.
- (WelcomeBackPromoRegistrationResult)
    promoRegistrationResultWithLastSessionEndTime:(NSDate*)lastSessionEndTime
                                  timeSinceActive:
                                      (base::TimeDelta)timeSinceActive {
  if (!lastSessionEndTime) {
    return WelcomeBackPromoRegistrationResult::kFailureSessionEndTimeNil;
  }
  if (timeSinceActive <= base::Days(28)) {
    return WelcomeBackPromoRegistrationResult::
        kFailureTimeSinceActiveLimitNotMet;
  }
  if (GetWelcomeBackEligibleItems().size() < kMinEligibleFeatures) {
    return WelcomeBackPromoRegistrationResult::
        kFailureMinEligibleFeaturesNotMet;
  }
  return WelcomeBackPromoRegistrationResult::kSuccess;
}

// Logs each missing promoted Welcome Back feature to a histogram with
// granular buckets.
- (void)logMissingWelcomeBackFeatures {
  std::vector<BestFeaturesItemType> eligibleItems =
      GetWelcomeBackEligibleItems();
  const BestFeaturesItemType allWelcomeBackFeatures[] = {
      BestFeaturesItemType::kLensSearch,
      BestFeaturesItemType::kEnhancedSafeBrowsing,
      BestFeaturesItemType::kLockedIncognitoTabs,
      BestFeaturesItemType::kSaveAndAutofillPasswords,
      BestFeaturesItemType::kTabGroups,
      BestFeaturesItemType::kPriceTrackingAndInsights,
      BestFeaturesItemType::kAutofillPasswordsInOtherApps,
      BestFeaturesItemType::kSharePasswordsWithFamily,
  };
  for (BestFeaturesItemType item : allWelcomeBackFeatures) {
    if (!std::ranges::contains(eligibleItems, item)) {
      base::UmaHistogramEnumeration(
          kWelcomeBackPromoRegistrationMissingFeatureHistogram, item);
    }
  }
}

@end

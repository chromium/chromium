// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/profile/welcome_back_screen_profile_agent.h"

#import <algorithm>

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/time/time.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/password_manager/core/browser/password_manager_util.h"
#import "components/prefs/pref_service.h"
#import "components/previous_session_info/previous_session_info.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
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
#import "ios/chrome/browser/shared/model/utils/first_run_util.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/welcome_back/metrics/welcome_back_metrics.h"
#import "ios/chrome/browser/welcome_back/model/features.h"
#import "ios/chrome/browser/welcome_back/model/welcome_back_prefs.h"

namespace {
// Minimum number of features required to show the promo.
constexpr size_t kMinEligibleFeatures = 2;

// Lookback window for active days tracking (28 days in the past + today).
constexpr uint32_t kActiveDaysTrackingWindow = 29;

// Histogram names.
const char kWelcomeBackActiveDaysInPast28DaysHistogram[] =
    "IOS.WelcomeBack.ActiveDaysInPast28Days";
const char kWelcomeBackDaysSinceActiveHistogram[] =
    "IOS.WelcomeBack.DaysSinceActive";
const char kWelcomeBackDaysSinceActiveNotFeatureFlagGuardedHistogram[] =
    "IOS.WelcomeBack.DaysSinceActiveNotFeatureFlagGuarded";
const char kWelcomeBackPromoRegistrationResultHistogram[] =
    "IOS.WelcomeBack.PromoRegistrationResult";
const char kWelcomeBackPromoRegistrationMissingFeatureHistogram[] =
    "IOS.WelcomeBack.PromoRegistration.MissingFeature";
}  // namespace

@implementation WelcomeBackScreenProfileAgent {
  // Whether the agent has started an asynchronous initialization flow. If YES,
  // cleanup is deferred until the async flow completes.
  BOOL _asyncInitializationInProgress;
}

#pragma mark - ProfileStateObserver

- (void)profileState:(ProfileState*)profileState
    didTransitionToInitStage:(ProfileInitStage)nextInitStage
               fromInitStage:(ProfileInitStage)fromInitStage {
  if (nextInitStage != ProfileInitStage::kFinal) {
    return;
  }

  ProfileIOS* profile = profileState.profile;
  DCHECK(profile);

  [self recordDaysSinceActiveHistogramWithName:
            kWelcomeBackDaysSinceActiveNotFeatureFlagGuardedHistogram];

  _asyncInitializationInProgress = NO;
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
          IdentityManagerFactory::GetForProfile(profile);
      if (identityManager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
        [self maybeRegisterPromo];
      }
      break;
  }

  // Defer cleanup if `maybeRegisterPromo` initiated an asynchronous flow.
  if (!_asyncInitializationInProgress) {
    [self cleanup];
  }
}

#pragma mark - Private

// Evaluates Welcome Back promo registration criteria. Registers or
// deregisters the promo accordingly, or defers evaluation if the feature
// engagement tracker needs to be initialized.
- (void)maybeRegisterPromo {
  // Mark Autofill feature as used if the Credential Provider Extension is
  // enabled on startup.
  PrefService* localState = GetApplicationContext()->GetLocalState();
  if (password_manager_util::IsCredentialProviderEnabledOnStartup(localState)) {
    MarkWelcomeBackFeatureUsed(
        BestFeaturesItemType::kAutofillPasswordsInOtherApps);
  }

  [self recordDaysSinceActiveHistogramWithName:
            kWelcomeBackDaysSinceActiveHistogram];

  if (ShouldWelcomeBackUseActiveDays()) {
    ProfileIOS* profile = self.profileState.profile;
    feature_engagement::Tracker* tracker =
        profile ? feature_engagement::TrackerFactory::GetForProfile(profile)
                : nullptr;
    if (!tracker) {
      base::UmaHistogramEnumeration(
          kWelcomeBackPromoRegistrationResultHistogram,
          WelcomeBackPromoRegistrationResult::kFailureTrackerInitialization);
      return;
    }

    // Wait for the tracker to be fully initialized.
    __weak WelcomeBackScreenProfileAgent* weakSelf = self;
    tracker->AddOnInitializedCallback(base::BindOnce(^(BOOL success) {
      [weakSelf onTrackerInitialized:success];
    }));
    // Set to YES to signal to `didTransitionToInitStage` that an async flow has
    // started, deferring immediate cleanup. The async callback will handle
    // cleanup when complete.
    _asyncInitializationInProgress = YES;
    return;
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
  [self handlePromoRegistrationResult:result];
}

// Helper called when the feature engagement tracker has been initialized.
- (void)onTrackerInitialized:(BOOL)success {
  ProfileIOS* profile = self.profileState.profile;
  feature_engagement::Tracker* tracker =
      profile ? feature_engagement::TrackerFactory::GetForProfile(profile)
              : nullptr;
  if (!success || !tracker) {
    [self cleanup];
    base::UmaHistogramEnumeration(
        kWelcomeBackPromoRegistrationResultHistogram,
        WelcomeBackPromoRegistrationResult::kFailureTrackerInitialization);
    return;
  }

  // Only log metrics and evaluate registration if Welcome Back is enabled and
  // the Best Features First Run screen is disabled.
  if (!IsWelcomeBackEnabled() ||
      base::FeatureList::IsEnabled(first_run::kBestFeaturesScreenInFirstRun)) {
    [self cleanup];
    return;
  }

  // Query the number of days with at least one active session in the last 28
  // days.
  int activeDaysInPast28Days = -1;
  for (const auto& [config, count] : tracker->ListEvents(
           feature_engagement::kIPHiOSActiveDaysTrackingFeature)) {
    if (config.name == feature_engagement::events::kChromeActiveSessionDay) {
      if (config.window == kActiveDaysTrackingWindow) {
        activeDaysInPast28Days = count;
        break;
      }
    }
  }

  WelcomeBackPromoRegistrationResult result =
      [self promoRegistrationResultWithActiveDays:activeDaysInPast28Days];
  [self handlePromoRegistrationResult:result];
  [self cleanup];
}

// Evaluates the active days count and logs/returns the corresponding
// registration outcome.
- (WelcomeBackPromoRegistrationResult)promoRegistrationResultWithActiveDays:
    (int)days {
  WelcomeBackPromoRegistrationResult result;
  if (IsFirstRun()) {
    // Since `lastSessionEndTime` is not checked, ensure that the user is not in
    // a first run experience.
    result = WelcomeBackPromoRegistrationResult::kFailureFirstRun;
  } else if (IsFirstRunRecent(base::Days(28))) {
    result = WelcomeBackPromoRegistrationResult::kFailureNotResurrectedUser;
  } else if (days < 0) {
    result = WelcomeBackPromoRegistrationResult::kFailureTrackerInitialization;
  } else if (days <= 1) {
    if (GetWelcomeBackEligibleItems().size() >= kMinEligibleFeatures) {
      result = WelcomeBackPromoRegistrationResult::kSuccess;
    } else {
      result =
          WelcomeBackPromoRegistrationResult::kFailureMinEligibleFeaturesNotMet;
    }
  } else {
    result =
        WelcomeBackPromoRegistrationResult::kFailureTimeSinceActiveLimitNotMet;
  }

  if (days >= 0) {
    base::UmaHistogramExactLinear(kWelcomeBackActiveDaysInPast28DaysHistogram,
                                  days, kActiveDaysTrackingWindow);
  }

  base::UmaHistogramEnumeration(kWelcomeBackPromoRegistrationResultHistogram,
                                result);
  return result;
}

// Handles the promo registration outcome, registering the promo if successful
// or logging missing features if required.
- (void)handlePromoRegistrationResult:
    (WelcomeBackPromoRegistrationResult)result {
  ProfileIOS* profile = self.profileState.profile;
  switch (result) {
    case WelcomeBackPromoRegistrationResult::kSuccess:
      PromosManagerFactory::GetForProfile(profile)
          ->RegisterPromoForSingleDisplay(promos_manager::Promo::WelcomeBack);
      break;
    case WelcomeBackPromoRegistrationResult::kFailureMinEligibleFeaturesNotMet:
      [self logMissingWelcomeBackFeatures];
      break;
    case WelcomeBackPromoRegistrationResult::kFailureTimeSinceActiveLimitNotMet:
    case WelcomeBackPromoRegistrationResult::kFailureSessionEndTimeNil:
    case WelcomeBackPromoRegistrationResult::kFailureFirstRun:
    case WelcomeBackPromoRegistrationResult::kFailureTrackerInitialization:
    case WelcomeBackPromoRegistrationResult::kFailureNotResurrectedUser:
      break;
  }
}

// Helper to safely defer cleanup during asynchronous feature
// engagement tracker evaluations, preventing premature deallocation. Stops
// observing the profile state and removes this agent from the profile state's
// agents list.
- (void)cleanup {
  ProfileState* profileState = self.profileState;
  [profileState removeObserver:self];
  if ([profileState.connectedAgents containsObject:self]) {
    [profileState removeAgent:self];
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

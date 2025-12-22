// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/profile/welcome_back_screen_profile_agent.h"

#import "base/check.h"
#import "base/time/time.h"
#import "components/password_manager/core/browser/password_manager_util.h"
#import "components/prefs/pref_service.h"
#import "components/previous_session_info/previous_session_info.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/first_run/public/best_features_item.h"
#import "ios/chrome/browser/first_run/ui_bundled/features.h"
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
#import "ios/chrome/browser/welcome_back/model/features.h"
#import "ios/chrome/browser/welcome_back/model/welcome_back_prefs.h"

namespace {
// Minimum number of features required to show the promo.
constexpr size_t kMinEligibleFeatures = 2;
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
  NSDate* lastSessionEndTime =
      [PreviousSessionInfo sharedInstance].sessionEndTime;
  base::TimeDelta timeSinceActive =
      lastSessionEndTime
          ? base::Time::Now() - base::Time::FromNSDate(lastSessionEndTime)
          : base::TimeDelta();

  // Mark Autofill feature as used if the Credential Provider Extension is
  // enabled on startup.
  PrefService* localState = GetApplicationContext()->GetLocalState();
  if (password_manager_util::IsCredentialProviderEnabledOnStartup(localState)) {
    MarkWelcomeBackFeatureUsed(
        BestFeaturesItemType::kAutofillPasswordsInOtherApps);
  }

  // Only register the promo if a user has been away for >28 days,
  // `kWelcomeBack` is enabled, `kBestFeaturesScreenInFirstRun` is
  // disabled, and there are at least two features eligible for display.
  if (timeSinceActive > base::Days(28) && IsWelcomeBackEnabled() &&
      !base::FeatureList::IsEnabled(first_run::kBestFeaturesScreenInFirstRun) &&
      GetWelcomeBackEligibleItems().size() >= kMinEligibleFeatures) {
    PromosManagerFactory::GetForProfile(self.profileState.profile)
        ->RegisterPromoForSingleDisplay(promos_manager::Promo::WelcomeBack);
  } else {
    PromosManagerFactory::GetForProfile(self.profileState.profile)
        ->DeregisterPromo(promos_manager::Promo::WelcomeBack);
  }
}

@end

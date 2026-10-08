// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webauthn/model/device_authorization_profile_agent.h"

#import <algorithm>
#import <optional>

#import "base/check.h"
#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/sequence_checker.h"
#import "base/time/time.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "components/signin/public/identity_manager/primary_account_change_event.h"
#import "components/webauthn/core/browser/device_authorization/device_authorization_features.h"
#import "components/webauthn/core/browser/device_authorization/device_authorization_service.h"
#import "components/webauthn/core/browser/device_authorization/device_authorization_types.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/app/deferred_initialization_runner.h"
#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/webauthn/model/ios_device_authorization_service_factory.h"

NSString* const kDeviceAuthorizationFetchBlockName =
    @"FetchDeviceAuthorizationKeys";

namespace {

// Returns whether the backoff allows a fetch now.
bool IsFetchAllowed(PrefService* pref_service) {
  const base::Time now = base::Time::Now();
  const base::Time next_fetch_time =
      pref_service->GetTime(prefs::kDeviceAuthorizationStartupFetchNextTime);
  // A next fetch time further away than the maximum backoff means the clock
  // was moved backwards. Ignore it rather than blocking fetches for too long.
  return next_fetch_time <= now ||
         next_fetch_time - now > kDeviceAuthorizationMaximumBackoff;
}

// Records a failed fetch, doubling the backoff.
void RecordFetchFailure(PrefService* pref_service) {
  const base::TimeDelta backoff = std::clamp(
      2 * pref_service->GetTimeDelta(
              prefs::kDeviceAuthorizationStartupFetchBackoff),
      kDeviceAuthorizationInitialBackoff, kDeviceAuthorizationMaximumBackoff);
  pref_service->SetTimeDelta(prefs::kDeviceAuthorizationStartupFetchBackoff,
                             backoff);
  pref_service->SetTime(prefs::kDeviceAuthorizationStartupFetchNextTime,
                        base::Time::Now() + backoff);
}

// Resets the backoff. Doesn't write anything if it is already reset.
void ResetFetchBackoff(PrefService* pref_service) {
  pref_service->ClearPref(prefs::kDeviceAuthorizationStartupFetchNextTime);
  pref_service->ClearPref(prefs::kDeviceAuthorizationStartupFetchBackoff);
}

}  // namespace

@interface DeviceAuthorizationProfileAgent () <IdentityManagerObserving>
@end

@implementation DeviceAuthorizationProfileAgent {
  // Null before `ProfileInitStage::kFinal` and after shutdown.
  raw_ptr<signin::IdentityManager> _identityManager;
  std::optional<signin::IdentityManagerObserverBridge> _identityObserverBridge;

  SEQUENCE_CHECKER(_sequenceChecker);
}

#pragma mark - NSObject

- (instancetype)init {
  CHECK(base::FeatureList::IsEnabled(
      webauthn::features::kFetchDeviceAuthorizationKeys));
  return [super init];
}

#pragma mark - ProfileStateObserver

- (void)profileState:(ProfileState*)profileState
    didTransitionToInitStage:(ProfileInitStage)nextInitStage
               fromInitStage:(ProfileInitStage)fromInitStage {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (nextInitStage != ProfileInitStage::kFinal) {
    return;
  }

  CHECK(profileState.profile);
  _identityManager =
      IdentityManagerFactory::GetForProfile(profileState.profile);
  _identityObserverBridge.emplace(_identityManager, self);
  [self scheduleFetchKeysIfNeeded];
}

#pragma mark - IdentityManagerObserving

- (void)primaryAccountDidChange:
    (const signin::PrimaryAccountChangeEvent&)event {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  const signin::PrimaryAccountChangeEvent::Type eventType =
      event.GetEventTypeFor(signin::ConsentLevel::kSignin);
  ProfileIOS* profile = self.profileState.profile;
  if (eventType == signin::PrimaryAccountChangeEvent::Type::kNone || !profile) {
    return;
  }

  // Reset backoff on account switch.
  ResetFetchBackoff(profile->GetPrefs());

  // Keys are cached per account, attempt fetch for a newly signed-in account.
  if (eventType == signin::PrimaryAccountChangeEvent::Type::kSet) {
    [self scheduleFetchKeysIfNeeded];
  }
}

- (void)identityManagerDidShutdown:(signin::IdentityManager*)identityManager {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  _identityObserverBridge.reset();
  _identityManager = nullptr;
}

#pragma mark - Private

// Enqueues `fetchKeysIfNeeded` on the deferred runner.
- (void)scheduleFetchKeysIfNeeded {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  __weak DeviceAuthorizationProfileAgent* weakSelf = self;
  [self.profileState.deferredRunner
      enqueueBlockNamed:kDeviceAuthorizationFetchBlockName
                  block:^{
                    [weakSelf fetchKeysIfNeeded];
                  }];
}

// Gets or fetches keys for the primary account, unless backing off after
// failed fetches. Valid cached keys are returned without a network fetch.
- (void)fetchKeysIfNeeded {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  ProfileIOS* profile = self.profileState.profile;
  if (!profile || !_identityManager ||
      !_identityManager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    return;
  }

  if (!IsFetchAllowed(profile->GetPrefs()) ||
      !base::FeatureList::IsEnabled(
          webauthn::features::kDeviceAuthorizationStartupSilentFetch)) {
    return;
  }

  GaiaId gaiaID =
      _identityManager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .gaia;
  __weak DeviceAuthorizationProfileAgent* weakSelf = self;
  IOSDeviceAuthorizationServiceFactory::GetForProfile(profile)->GetOrFetchKeys(
      base::BindOnce(^(webauthn::DeviceAuthFetchResult result) {
        [weakSelf didFetchKeys:result forAccount:gaiaID];
      }));
}

// Resets the backoff after a successful fetch for `gaiaID`, and backs off
// further otherwise. The rest of the result is ignored, as it is not actionable
// in the context of a startup fetch (no UI).
// TODO(crbug.com/405036154): Consider recording metrics.
- (void)didFetchKeys:(const webauthn::DeviceAuthFetchResult&)result
          forAccount:(const GaiaId&)gaiaID {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  ProfileIOS* profile = self.profileState.profile;
  // A result for a previous primary account must not affect the backoff, which
  // was reset for the current one.
  if (!profile || !_identityManager ||
      _identityManager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
              .gaia != gaiaID) {
    return;
  }

  PrefService* prefService = profile->GetPrefs();
  if (result.status() == webauthn::DeviceAuthFetchResult::Status::kSuccess) {
    ResetFetchBackoff(prefService);
  } else {
    RecordFetchFailure(prefService);
  }
}

@end

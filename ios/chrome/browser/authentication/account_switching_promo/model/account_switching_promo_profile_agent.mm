// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/account_switching_promo/model/account_switching_promo_profile_agent.h"

#import <memory>
#import <optional>
#import <utility>
#import <vector>

#import "base/callback_list.h"
#import "base/containers/flat_set.h"
#import "base/functional/bind.h"
#import "base/metrics/histogram_functions.h"
#import "base/time/time.h"
#import "base/values.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "components/signin/core/browser/account_preview_data_service.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "components/signin/public/base/signin_switches.h"
#import "components/signin/public/browser/account_switching_promo_metrics.h"
#import "components/signin/public/browser/account_switching_promo_prefs.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "components/signin/public/identity_manager/tribool.h"
#import "components/supervised_user/core/browser/family_link_user_capabilities.h"
#import "google_apis/gaia/core_account_id.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/profile/profile_state_observer.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/promos_manager/model/constants.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_activation_level.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_util.h"
#import "ios/chrome/browser/signin/model/account_preview_data_service_factory.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/system_identity.h"

namespace {
constexpr base::TimeDelta kMinDaysSinceLastSignin = base::Days(28);
// Whether the promo eligibility has already been evaluated at startup in this
// Chrome session. Global so that switching from the personal profile to a
// managed profile and back without restarting does not evaluate the promo a
// second time. Must not be set to `YES` by a managed profile.
BOOL gPromoAlreadyEvaluated = NO;
}  // namespace

@interface AccountSwitchingPromoProfileAgent () <IdentityManagerObserving>
@end

@implementation AccountSwitchingPromoProfileAgent {
  // Observers of the profile, set up by `-startObservingProfileIfNeeded` only
  // if the profile is the personal profile and supports account switching.
  // `_identityManagerObserver` is non-null iff the agent observes the profile.
  PrefChangeRegistrar _prefChangeRegistrar;
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserver;
  base::CallbackListSubscription _profileDestroyedSubscription;
}

+ (void)resetPromoAlreadyEvaluatedForTesting {
  gPromoAlreadyEvaluated = NO;
}

// Called when the account switching promo state preference changes.
// Deregisters the promo if it is no longer eligible.
- (void)accountSwitchingPromoStatePrefDidChange {
  [self deregisterPromoIfIneligible];
}

- (id<SystemIdentity>)targetIdentity {
  if (![self isPersonalProfile]) {
    return nil;
  }
  ProfileIOS* profile = self.profileState.profile;
  signin::AccountPreviewDataService* accountPreviewDataService =
      AccountPreviewDataServiceFactory::GetForProfile(profile);
  if (!accountPreviewDataService) {
    return nil;
  }
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForProfile(profile);
  id<SystemIdentity> primaryIdentity = authService->GetPrimaryIdentity();
  if (!primaryIdentity || authService->HasPrimaryIdentityManaged()) {
    return nil;
  }
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(profile);
  if (supervised_user::IsPrimaryAccountSubjectToParentalControls(
          identityManager) != signin::Tribool::kFalse) {
    return nil;
  }

  std::optional<signin::AccountPreviewDataService::AccountPreviewPreference>
      switchingPref =
          accountPreviewDataService->GetPreferredAccountForSwitching();
  // Only identities of this profile are considered, since the promo switches
  // the primary account of the current profile.
  if (!switchingPref.has_value() ||
      !identityManager->HasAccountWithRefreshToken(
          CoreAccountId::FromGaiaId(switchingPref->gaia_id))) {
    return nil;
  }
  id<SystemIdentity> targetIdentity =
      ChromeAccountManagerServiceFactory::GetForProfile(profile)
          ->GetIdentityOnDeviceWithGaiaID(switchingPref->gaia_id);
  if (!targetIdentity || targetIdentity.gaiaId == primaryIdentity.gaiaId) {
    // Early return: no distinct switching candidate identity in the profile.
    return nil;
  }
  AccountInfo targetAccountInfo =
      identityManager->FindExtendedAccountInfoByGaiaId(targetIdentity.gaiaId);
  if (targetAccountInfo.IsManaged() != signin::Tribool::kFalse) {
    // Early return: never offer to switch unless the target account is known
    // not to be managed, since switching to a managed account would trigger a
    // profile switch (unsupported by the promo).
    return nil;
  }

  return targetIdentity;
}

#pragma mark - ObservingProfileAgent

- (void)setProfileState:(ProfileState*)profileState {
  [super setProfileState:profileState];
  if (!profileState) {
    // The agent was removed from its profile state.
    [self profileDestroyed];
  }
}

#pragma mark - ProfileStateObserver

- (void)profileState:(ProfileState*)profileState
    didTransitionToInitStage:(ProfileInitStage)nextInitStage
               fromInitStage:(ProfileInitStage)fromInitStage {
  [self startObservingProfileIfNeeded];
  if (nextInitStage == ProfileInitStage::kFinal) {
    [self maybeRecordRetentionMetrics];
  }
  [self maybeEvaluatePromoAtStartup];
}

#pragma mark - SceneObservingProfileAgent

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  if (level == SceneActivationLevelForegroundActive) {
    [self maybeRecordRetentionMetrics];
  }
  [self maybeEvaluatePromoAtStartup];
}

#pragma mark - IdentityManagerObserving

- (void)primaryAccountDidChange:
    (const signin::PrimaryAccountChangeEvent&)event {
  ProfileIOS* profile = self.profileState.profile;
  if (!profile) {
    // Early return: the profile is being destroyed.
    return;
  }
  // TODO(crbug.com/571565524): This tracks only signing in with a personal
  // account (since this agent only observes the personal profile). Check
  // whether signing in with a managed account should also be tracked.
  if (event.GetEventTypeFor(signin::ConsentLevel::kSignin) ==
      signin::PrimaryAccountChangeEvent::Type::kSet) {
    feature_engagement::TrackerFactory::GetForProfile(profile)->NotifyEvent(
        feature_engagement::events::kAccountSwitchingPrimaryAccountChanged);
  }
  [self maybeRecordRetentionMetrics];
  [self deregisterPromoIfIneligible];
}

- (void)batchOfRefreshTokenStateChangesDidEnd {
  [self deregisterPromoIfIneligible];
}

- (void)extendedAccountInfoDidUpdate:(const AccountInfo&)info {
  [self deregisterPromoIfIneligible];
}

- (void)accountsOnDeviceDidChange {
  [self pruneRemovedAccountsFromPrefs];
  [self deregisterPromoIfIneligible];
}

#pragma mark - Private

// Starts observing the profile of `self.profileState` if it is loaded, is the
// personal profile and supports account switching, and if the agent does not
// already observe it. Otherwise, does nothing, so that the agent stays inactive
// for other profiles.
- (void)startObservingProfileIfNeeded {
  ProfileIOS* profile = self.profileState.profile;
  if (_identityManagerObserver || !profile || !IsPersonalProfile(profile) ||
      !AccountPreviewDataServiceFactory::GetForProfile(profile)) {
    return;
  }

  _identityManagerObserver =
      std::make_unique<signin::IdentityManagerObserverBridge>(
          IdentityManagerFactory::GetForProfile(profile), self);
  _prefChangeRegistrar.Init(profile->GetPrefs());
  __weak __typeof(self) weakSelf = self;
  _prefChangeRegistrar.Add(
      signin::kAccountSwitchingPromoStatePref,
      base::BindRepeating(
          [](AccountSwitchingPromoProfileAgent* agent) {
            [agent accountSwitchingPromoStatePrefDidChange];
          },
          weakSelf));
  _prefChangeRegistrar.Add(prefs::kAccountPreviewSwitchingPreference,
                           base::BindRepeating(
                               [](AccountSwitchingPromoProfileAgent* agent) {
                                 [agent deregisterPromoIfIneligible];
                               },
                               weakSelf));
  _profileDestroyedSubscription =
      profile->RegisterProfileDestroyedCallback(base::BindOnce(
          [](AccountSwitchingPromoProfileAgent* agent) {
            [agent profileDestroyed];
          },
          weakSelf));
}

// Clears all profile-scoped observers when the profile is destroyed or when the
// agent is removed from its profile state.
- (void)profileDestroyed {
  _profileDestroyedSubscription = {};
  _prefChangeRegistrar.RemoveAll();
  _identityManagerObserver.reset();
}

// Returns whether the profile is currently the personal profile (`YES` if
// personal, `NO` otherwise). This is evaluated each time rather than once at
// creation, because the personal profile can be converted into a managed
// profile while Chrome runs: when the user signs in with a managed account and
// chooses to keep the browsing data of the personal profile in their managed
// profile (see `MakePersonalProfileManagedWithGaiaID()`). The other conversion,
// forced migration of a managed primary account signed in before multi-profile
// support, can happen at startup, but the agent does not observe the profile in
// that case since the primary account is managed.
// Returns `NO` if the profile is not loaded or is being destroyed.
- (BOOL)isPersonalProfile {
  ProfileIOS* profile = self.profileState.profile;
  return profile && IsPersonalProfile(profile);
}

// Deregisters `promos_manager::Promo::AccountSwitching` from the profile's
// `PromosManager`.
- (void)deregisterAccountSwitchingPromo {
  ProfileIOS* profile = self.profileState.profile;
  if (!profile) {
    // Early return: the profile is being destroyed.
    return;
  }
  PromosManagerFactory::GetForProfile(profile)->DeregisterPromo(
      promos_manager::Promo::AccountSwitching);
}

// Prunes promo preferences for accounts removed from the device, or clears
// `kAccountSwitchingPromoStatePref` if the profile is no longer the personal
// profile.
- (void)pruneRemovedAccountsFromPrefs {
  ProfileIOS* profile = self.profileState.profile;
  if (!profile) {
    // Early return: the profile is being destroyed.
    return;
  }
  PrefService* prefService = profile->GetPrefs();
  if (prefService->GetDict(signin::kAccountSwitchingPromoStatePref).empty()) {
    // Early return: no promo state is stored yet.
    return;
  }
  if (!IsPersonalProfile(profile)) {
    // Clear stored state if the personal profile was converted into a managed
    // profile via `MakePersonalProfileManagedWithGaiaID()`.
    prefService->ClearPref(signin::kAccountSwitchingPromoStatePref);
    return;
  }

  std::vector<GaiaId> activeGaiaIds;
  for (const AccountInfo& account :
       IdentityManagerFactory::GetForProfile(profile)->GetAccountsOnDevice()) {
    activeGaiaIds.push_back(account.GetGaiaId());
  }
  signin::DeleteAccountSwitchingPromoStateForRemovedAccounts(
      *prefService, base::flat_set<GaiaId>(std::move(activeGaiaIds)));
}

// Prunes any accounts removed from the device (so removing the primary account
// from the device does not record a switch away) and records account switching
// 24-hour retention metrics if a pending evaluation is ready and the profile is
// personal.
- (void)maybeRecordRetentionMetrics {
  if (!_identityManagerObserver) {
    // Early return: the agent does not observe the profile.
    return;
  }
  [self pruneRemovedAccountsFromPrefs];

  if (![self isPersonalProfile]) {
    // Early return: account switching promo state is only stored for the
    // personal profile.
    return;
  }

  ProfileIOS* profile = self.profileState.profile;
  id<SystemIdentity> primaryIdentity =
      AuthenticationServiceFactory::GetForProfile(profile)
          ->GetPrimaryIdentity();
  signin::MaybeRecordAccountSwitchingRetention(
      *profile->GetPrefs(),
      primaryIdentity ? primaryIdentity.gaiaId : GaiaId());
}

// Returns whether the promo may be offered for the current primary account.
- (BOOL)isPromoEligible {
  id<SystemIdentity> targetIdentity = self.targetIdentity;
  if (!targetIdentity) {
    // The profile or primary account is ineligible, or there is no distinct
    // switching candidate identity in the profile.
    return NO;
  }
  // `targetIdentity` is non-nil only if the profile is loaded and has a
  // primary identity.
  ProfileIOS* profile = self.profileState.profile;
  id<SystemIdentity> primaryIdentity =
      AuthenticationServiceFactory::GetForProfile(profile)
          ->GetPrimaryIdentity();

  if (switches::kEnableAccountPreviewSwitchingAccountSkipCooldown.Get()) {
    return YES;
  }

  PrefService* prefService = profile->GetPrefs();
  // TODO(crbug.com/571565524): This check is redundant with the Feature
  // Engagement Tracker `kAccountSwitchingPrimaryAccountChanged` rule and can
  // be removed once this code has been on stable for more than 28 days (plus a
  // safety margin for people who do not update regularly).
  if (base::Time::Now() - prefService->GetTime(prefs::kLastSigninTimestamp) <
      kMinDaysSinceLastSignin) {
    // The last sign-in occurred less than 28 days ago.
    return NO;
  }

  // Per-account-pair rules (explicit dismissals cap or reverse-switch
  // suppression).
  return signin::CanShowAccountSwitchingPromo(
      *prefService, primaryIdentity.gaiaId, targetIdentity.gaiaId);
}

// Deregisters the promo if it is no longer eligible, so that `PromosManager`
// never queries `feature_engagement::Tracker::ShouldTriggerHelpUI()` for it.
// Never registers the promo, which is only done at startup.
- (void)deregisterPromoIfIneligible {
  if (![self isPromoEligible]) {
    [self deregisterAccountSwitchingPromo];
  }
}

// Evaluates the promo eligibility the first time a scene of the profile becomes
// foreground-active at `ProfileInitStage::kFinal`, and registers the promo with
// `PromosManager` if eligible. Does nothing on later calls, so that the promo
// is only registered once per profile at Chrome startup.
- (void)maybeEvaluatePromoAtStartup {
  if (gPromoAlreadyEvaluated || !_identityManagerObserver) {
    // Early return: the promo was already evaluated at startup, or the agent
    // does not observe the profile (including managed profiles).
    return;
  }

  if (self.profileState.initStage < ProfileInitStage::kFinal) {
    // Early return: wait for `ProfileInitStage::kFinal`.
    return;
  }

  if (!self.profileState.foregroundActiveScene) {
    // Early return: wait for a scene of the profile to be foreground-active.
    return;
  }

  // Only reached when `_identityManagerObserver` is non-null (i.e. personal
  // profile), so a managed profile never sets `gPromoAlreadyEvaluated` to
  // `YES`.
  gPromoAlreadyEvaluated = YES;

  if (![self isPromoEligible]) {
    [self deregisterAccountSwitchingPromo];
    return;
  }

  base::UmaHistogramEnumeration(
      signin::kAccountSwitchingPromoEventsHistogram,
      signin::AccountSwitchingPromoEvent::kEligibleOnStartup);
  PromosManagerFactory::GetForProfile(self.profileState.profile)
      ->RegisterPromoForSingleDisplay(promos_manager::Promo::AccountSwitching);
}

@end

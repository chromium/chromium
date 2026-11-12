// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/account_switching_promo/model/account_switching_promo_scene_agent.h"

#import <memory>
#import <optional>
#import <utility>
#import <vector>

#import "base/check.h"
#import "base/containers/flat_set.h"
#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/not_fatal_until.h"
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
#import "ios/chrome/browser/authentication/account_switching_promo/model/account_switching_promo_metrics.h"
#import "ios/chrome/browser/authentication/account_switching_promo/model/account_switching_promo_prefs.h"
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
}  // namespace

@interface AccountSwitchingPromoSceneAgent () <IdentityManagerObserving,
                                               ProfileStateObserver>
@end

@implementation AccountSwitchingPromoSceneAgent {
  raw_ptr<PromosManager> _promosManager;
  raw_ptr<AuthenticationService> _authService;
  raw_ptr<signin::IdentityManager> _identityManager;
  raw_ptr<ChromeAccountManagerService> _accountManagerService;
  raw_ptr<signin::AccountPreviewDataService> _accountPreviewDataService;
  raw_ptr<PrefService> _prefService;
  raw_ptr<feature_engagement::Tracker> _featureEngagementTracker;
  PrefChangeRegistrar _prefChangeRegistrar;
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserver;
  // Whether the promo eligibility has already been evaluated at startup.
  // TODO(crbug.com/571565524): Convert `AccountSwitchingPromoSceneAgent` into a
  // `SceneObservingProfileAgent`.
  BOOL _promoEvaluatedAtStartup;
}

+ (instancetype)agentWithProfile:(ProfileIOS*)profile {
  CHECK(profile, base::NotFatalUntil::M170);
  if (!profile || !IsPersonalProfile(profile)) {
    return nil;
  }
  signin::AccountPreviewDataService* accountPreviewDataService =
      AccountPreviewDataServiceFactory::GetForProfile(profile);
  if (!accountPreviewDataService) {
    return nil;
  }
  if (!base::FeatureList::IsEnabled(
          switches::kEnableAccountPreviewSwitchingAccount)) {
    return nil;
  }
  PromosManager* promosManager = PromosManagerFactory::GetForProfile(profile);
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForProfile(profile);
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(profile);
  ChromeAccountManagerService* accountManagerService =
      ChromeAccountManagerServiceFactory::GetForProfile(profile);
  PrefService* prefService = profile->GetPrefs();
  feature_engagement::Tracker* featureEngagementTracker =
      feature_engagement::TrackerFactory::GetForProfile(profile);
  CHECK(promosManager, base::NotFatalUntil::M170);
  CHECK(authService, base::NotFatalUntil::M170);
  CHECK(identityManager, base::NotFatalUntil::M170);
  CHECK(accountManagerService, base::NotFatalUntil::M170);
  CHECK(prefService, base::NotFatalUntil::M170);
  CHECK(featureEngagementTracker, base::NotFatalUntil::M170);
  if (!promosManager || !authService || !identityManager ||
      !accountManagerService || !prefService || !featureEngagementTracker) {
    return nil;
  }
  return [[self alloc] initWithPromosManager:promosManager
                                 authService:authService
                             identityManager:identityManager
                       accountManagerService:accountManagerService
                   accountPreviewDataService:accountPreviewDataService
                                 prefService:prefService
                    featureEngagementTracker:featureEngagementTracker];
}

// Initializes the scene agent.
// `promosManager`: Promos manager with which the promo is registered. Must not
// be null.
// `authService`: Service used to check the primary identity and management
// status. Must not be null.
// `identityManager`: Identity manager used to check parental controls and
// observe sign-in changes. Must not be null.
// `accountManagerService`: Service used to convert a Gaia ID into
// `id<SystemIdentity>`. Must not be null.
// `accountPreviewDataService`: Data service providing the preferred switching
// target. Must not be null.
// `prefService`: Personal profile pref service used to check promo cooldowns
// and caps. Must not be null.
// `featureEngagementTracker`: Tracker notified when the primary account
// changes. Must not be null.
- (instancetype)initWithPromosManager:(PromosManager*)promosManager
                          authService:(AuthenticationService*)authService
                      identityManager:(signin::IdentityManager*)identityManager
                accountManagerService:
                    (ChromeAccountManagerService*)accountManagerService
            accountPreviewDataService:
                (signin::AccountPreviewDataService*)accountPreviewDataService
                          prefService:(PrefService*)prefService
             featureEngagementTracker:
                 (feature_engagement::Tracker*)featureEngagementTracker {
  CHECK(promosManager);
  CHECK(authService);
  CHECK(identityManager);
  CHECK(accountManagerService);
  CHECK(accountPreviewDataService);
  CHECK(prefService);
  CHECK(featureEngagementTracker);
  self = [super init];
  if (self) {
    _promosManager = promosManager;
    _authService = authService;
    _identityManager = identityManager;
    _accountManagerService = accountManagerService;
    _accountPreviewDataService = accountPreviewDataService;
    _prefService = prefService;
    _featureEngagementTracker = featureEngagementTracker;
    _identityManagerObserver =
        std::make_unique<signin::IdentityManagerObserverBridge>(identityManager,
                                                                self);
    _prefChangeRegistrar.Init(prefService);
    __weak __typeof(self) weakSelf = self;
    _prefChangeRegistrar.Add(
        signin::kAccountSwitchingPromoStatePref,
        base::BindRepeating(
            [](AccountSwitchingPromoSceneAgent* agent) {
              [agent accountSwitchingPromoStatePrefDidChange];
            },
            weakSelf));
    _prefChangeRegistrar.Add(prefs::kAccountPreviewSwitchingPreference,
                             base::BindRepeating(
                                 [](AccountSwitchingPromoSceneAgent* agent) {
                                   [agent deregisterPromoIfIneligible];
                                 },
                                 weakSelf));
  }
  return self;
}

// Called when the account switching promo state preference changes.
// Deregisters the promo if it is no longer eligible.
- (void)accountSwitchingPromoStatePrefDidChange {
  [self deregisterPromoIfIneligible];
}

- (id<SystemIdentity>)targetIdentity {
  CHECK(_authService);
  if (![self isPersonalProfile]) {
    return nil;
  }
  id<SystemIdentity> primaryIdentity = _authService->GetPrimaryIdentity();
  if (!primaryIdentity || _authService->HasPrimaryIdentityManaged()) {
    return nil;
  }
  if (supervised_user::IsPrimaryAccountSubjectToParentalControls(
          _identityManager) != signin::Tribool::kFalse) {
    return nil;
  }

  std::optional<signin::AccountPreviewDataService::AccountPreviewPreference>
      switchingPref =
          _accountPreviewDataService->GetPreferredAccountForSwitching();
  // Only identities of this profile are considered, since the promo switches
  // the primary account of the current profile.
  if (!switchingPref.has_value() ||
      !_identityManager->HasAccountWithRefreshToken(
          CoreAccountId::FromGaiaId(switchingPref->gaia_id))) {
    return nil;
  }
  id<SystemIdentity> targetIdentity =
      _accountManagerService->GetIdentityOnDeviceWithGaiaID(
          switchingPref->gaia_id);
  if (!targetIdentity || targetIdentity.gaiaId == primaryIdentity.gaiaId) {
    // Early return: no distinct switching candidate identity in the profile.
    return nil;
  }

  return targetIdentity;
}

#pragma mark - ObservingSceneAgent

- (void)setSceneState:(SceneState*)sceneState {
  [super setSceneState:sceneState];
  // Replays `-profileState:didTransitionToInitStage:fromInitStage:` if the
  // profile is already initializing, which prunes the prefs and records the
  // retention metrics once `ProfileInitStage::kFinal` is reached.
  [self.sceneState.profileState addObserver:self];
}

#pragma mark - ProfileStateObserver

- (void)profileState:(ProfileState*)profileState
    didTransitionToInitStage:(ProfileInitStage)nextInitStage
               fromInitStage:(ProfileInitStage)fromInitStage {
  if (nextInitStage == ProfileInitStage::kFinal) {
    [self maybeRecordRetentionMetrics];
  }
  [self maybeEvaluatePromoAtStartup];
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  if (level == SceneActivationLevelForegroundActive) {
    [self maybeRecordRetentionMetrics];
  }
  [self maybeEvaluatePromoAtStartup];
}

- (void)sceneStateDidDisableUI:(SceneState*)sceneState {
  _prefChangeRegistrar.RemoveAll();
  _identityManagerObserver.reset();
  _promosManager = nullptr;
  _authService = nullptr;
  _identityManager = nullptr;
  _accountManagerService = nullptr;
  _accountPreviewDataService = nullptr;
  _prefService = nullptr;
  _featureEngagementTracker = nullptr;
  [self.sceneState.profileState removeObserver:self];
  [sceneState removeObserver:self];
}

#pragma mark - IdentityManagerObserving

- (void)primaryAccountDidChange:
    (const signin::PrimaryAccountChangeEvent&)event {
  CHECK(_featureEngagementTracker, base::NotFatalUntil::M170);
  if (!_featureEngagementTracker) {
    return;
  }
  // TODO(crbug.com/571565524): This tracks only signing in with a personal
  // account (since this agent is only created for the personal profile). Check
  // whether signing in with a managed account should also be tracked.
  if (event.GetEventTypeFor(signin::ConsentLevel::kSignin) ==
      signin::PrimaryAccountChangeEvent::Type::kSet) {
    _featureEngagementTracker->NotifyEvent(
        feature_engagement::events::kIOSAccountSwitchingPrimaryAccountChanged);
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

// Returns whether the profile is currently the personal profile (`YES` if
// personal, `NO` otherwise). This is evaluated each time rather than once at
// creation, because the personal profile can be converted into a managed
// profile while Chrome runs: when the user signs in with a managed account and
// chooses to keep the browsing data of the personal profile in their managed
// profile (see `MakePersonalProfileManagedWithGaiaID()`). The other conversion,
// forced migration of a managed primary account signed in before multi-profile
// support, can happen at startup, but the agent is not created in that case
// since the primary account is managed.
- (BOOL)isPersonalProfile {
  ProfileIOS* profile = self.sceneState.profileState.profile;
  CHECK(profile);
  return IsPersonalProfile(profile);
}

// Deregisters `promos_manager::Promo::AccountSwitching` from `_promosManager`.
- (void)deregisterAccountSwitchingPromo {
  _promosManager->DeregisterPromo(promos_manager::Promo::AccountSwitching);
}

// Prunes promo preferences for accounts removed from the device, or clears
// `kAccountSwitchingPromoStatePref` if the profile is no longer the personal
// profile.
- (void)pruneRemovedAccountsFromPrefs {
  if (_prefService->GetDict(signin::kAccountSwitchingPromoStatePref).empty()) {
    // Early return: no promo state is stored yet.
    return;
  }
  if (![self isPersonalProfile]) {
    // Clear stored state if the personal profile was converted into a managed
    // profile via `MakePersonalProfileManagedWithGaiaID()`.
    _prefService->ClearPref(signin::kAccountSwitchingPromoStatePref);
    return;
  }

  std::vector<GaiaId> activeGaiaIds;
  for (const AccountInfo& account : _identityManager->GetAccountsOnDevice()) {
    activeGaiaIds.push_back(account.GetGaiaId());
  }
  signin::DeleteAccountSwitchingPromoStateForRemovedAccounts(
      *_prefService, base::flat_set<GaiaId>(std::move(activeGaiaIds)));
}

// Prunes any accounts removed from the device (so removing the primary account
// from the device does not record a switch away) and records account switching
// 24-hour retention metrics if a pending evaluation is ready and the profile is
// personal.
- (void)maybeRecordRetentionMetrics {
  [self pruneRemovedAccountsFromPrefs];

  if (![self isPersonalProfile]) {
    // Early return: account switching promo state is only stored for the
    // personal profile.
    return;
  }

  id<SystemIdentity> primaryIdentity = _authService->GetPrimaryIdentity();
  signin::MaybeRecordAccountSwitchingRetention(
      *_prefService, primaryIdentity ? primaryIdentity.gaiaId : GaiaId());
}

// Returns whether the promo may be offered for the current primary account.
- (BOOL)isPromoEligible {
  id<SystemIdentity> targetIdentity = self.targetIdentity;
  id<SystemIdentity> primaryIdentity = _authService->GetPrimaryIdentity();
  if (!targetIdentity || !primaryIdentity) {
    // The profile or primary account is ineligible, or there is no distinct
    // switching candidate identity in the profile.
    return NO;
  }

  if (switches::kEnableAccountPreviewSwitchingAccountSkipCooldown.Get()) {
    return YES;
  }

  // TODO(crbug.com/571565524): This check is redundant with the Feature
  // Engagement Tracker `kIOSAccountSwitchingPrimaryAccountChanged` rule and can
  // be removed once this code has been on stable for more than 28 days (plus a
  // safety margin for people who do not update regularly).
  if (base::Time::Now() - _prefService->GetTime(prefs::kLastSigninTimestamp) <
      kMinDaysSinceLastSignin) {
    // The last sign-in occurred less than 28 days ago.
    return NO;
  }

  // Per-account-pair rules (explicit dismissals cap or reverse-switch
  // suppression).
  return signin::CanShowAccountSwitchingPromo(
      *_prefService, primaryIdentity.gaiaId, targetIdentity.gaiaId);
}

// Deregisters the promo if it is no longer eligible, so that `PromosManager`
// never queries `feature_engagement::Tracker::ShouldTriggerHelpUI()` for it.
// Never registers the promo, which is only done at startup.
- (void)deregisterPromoIfIneligible {
  if (![self isPromoEligible]) {
    [self deregisterAccountSwitchingPromo];
  }
}

// Evaluates the promo eligibility the first time the scene becomes
// foreground-active at `ProfileInitStage::kFinal`, and registers the promo with
// `PromosManager` if eligible. Does nothing on later calls, so that the promo
// is only registered at Chrome startup.
- (void)maybeEvaluatePromoAtStartup {
  if (_promoEvaluatedAtStartup) {
    // Early return: the promo was already evaluated at startup.
    return;
  }

  if (self.sceneState.profileState.initStage < ProfileInitStage::kFinal) {
    // Early return: wait for `ProfileInitStage::kFinal`.
    return;
  }

  if (self.sceneState.activationLevel < SceneActivationLevelForegroundActive) {
    // Early return: wait for the scene to be foreground-active.
    return;
  }

  _promoEvaluatedAtStartup = YES;

  if (![self isPromoEligible]) {
    [self deregisterAccountSwitchingPromo];
    return;
  }

  base::UmaHistogramEnumeration(
      signin::kAccountSwitchingPromoEventsHistogram,
      signin::AccountSwitchingPromoEvent::kEligibleOnStartup);
  _promosManager->RegisterPromoForSingleDisplay(
      promos_manager::Promo::AccountSwitching);
}

@end

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/coordinator/signin_account_capabilities_scene_agent.h"

#import <set>
#import <vector>

#import "base/barrier_closure.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/location.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "base/timer/timer.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/identity_manager/account_capabilities.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/profile/profile_state_observer.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_utils.h"
#import "ios/chrome/browser/scoped_ui_blocker/ui_bundled/scoped_ui_blocker.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_ui_provider.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/coordinator/age_mismatch_signout_coordinator.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/signin_util.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"
#import "ios/chrome/browser/signin/model/system_identity_manager_observer_bridge.h"
#import "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace {

// The interval after which the External Privacy Context becomes stale and needs
// to be built again.
constexpr base::TimeDelta kExternalPrivacyContextStalenessInterval =
    base::Days(1);

}  // namespace

@interface SigninAccountCapabilitiesSceneAgent () <
    IdentityManagerObserverBridgeDelegate,
    ProfileStateObserver,
    SystemIdentityManagerObserving,
    UIBlockerManagerObserver>
@end

@implementation SigninAccountCapabilitiesSceneAgent {
  // SceneUIProvider that provides the scene UI objects.
  __weak id<SceneUIProvider> _sceneUIProvider;

  // The set of Gaia IDs for which the external privacy context has been built.
  absl::flat_hash_map<GaiaId, base::Time, GaiaId::Hash> _handledIdentities;

  std::unique_ptr<SystemIdentityManagerObserverBridge>
      _systemIdentityManagerObserver;

  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserver;

  // Coordinator for the Age Mismatch prompt.
  AgeMismatchSignoutCoordinator* _ageMismatchSignoutCoordinator;

  // Timer to periodically rebuild the External Privacy Contexts
  base::RepeatingTimer _refreshTimer;
}

- (instancetype)initWithSceneUIProvider:(id<SceneUIProvider>)sceneUIProvider {
  self = [super init];
  if (self) {
    _sceneUIProvider = sceneUIProvider;
    _systemIdentityManagerObserver =
        std::make_unique<SystemIdentityManagerObserverBridge>(
            GetApplicationContext()->GetSystemIdentityManager(), self);
  }
  return self;
}

#pragma mark - ObservingSceneAgent

- (void)setSceneState:(SceneState*)sceneState {
  [super setSceneState:sceneState];
  [self.sceneState.profileState addObserver:self];
  [self.sceneState.profileState addUIBlockerManagerObserver:self];

  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(
          self.sceneState.profileState.profile);
  if (identityManager) {
    _identityManagerObserver =
        std::make_unique<signin::IdentityManagerObserverBridge>(identityManager,
                                                                self);
  }

  __weak __typeof(self) weakSelf = self;
  _refreshTimer.Start(FROM_HERE, kExternalPrivacyContextStalenessInterval,
                      base::BindRepeating(^{
                        [weakSelf fetchCapabilitiesForUnhandledIdentities];
                      }));
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  [self fetchCapabilitiesForUnhandledIdentities];
}

- (void)sceneStateDidDisableUI:(SceneState*)sceneState {
  [self.sceneState.profileState removeUIBlockerManagerObserver:self];
  [self.sceneState.profileState removeObserver:self];
  [self.sceneState removeObserver:self];
  _systemIdentityManagerObserver.reset();
  _identityManagerObserver.reset();
  _refreshTimer.Stop();
  [_ageMismatchSignoutCoordinator stop];
  _ageMismatchSignoutCoordinator = nil;
}

- (void)sceneStateDidHideModalOverlay:(SceneState*)sceneState {
  [self fetchCapabilitiesForUnhandledIdentities];
}

#pragma mark - ProfileStateObserver

- (void)profileState:(ProfileState*)profileState
    didTransitionToInitStage:(ProfileInitStage)nextInitStage
               fromInitStage:(ProfileInitStage)fromInitStage {
  // The services (including e.g. IdentityManager) are available at this stage.
  if (nextInitStage >= ProfileInitStage::kProfileLoaded &&
      !_identityManagerObserver) {
    signin::IdentityManager* identityManager =
        IdentityManagerFactory::GetForProfile(
            self.sceneState.profileState.profile);
    _identityManagerObserver =
        std::make_unique<signin::IdentityManagerObserverBridge>(identityManager,
                                                                self);
  }
  [self fetchCapabilitiesForUnhandledIdentities];
}

#pragma mark - SystemIdentityManagerObserving

- (void)onIdentityListChanged {
  [self fetchCapabilitiesForUnhandledIdentities];
}

#pragma mark - IdentityManagerObserverBridgeDelegate

- (void)onExtendedAccountInfoUpdated:(const AccountInfo&)info {
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(
          self.sceneState.profileState.profile);
  if (info.account_id !=
      identityManager->GetPrimaryAccountId(signin::ConsentLevel::kSignin)) {
    return;
  }

  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForProfile(
          self.sceneState.profileState.profile);
  if (!authenticationService) {
    return;
  }

  if (info.capabilities.can_sign_in_to_chrome() == signin::Tribool::kFalse) {
    authenticationService->SignOut(
        signin_metrics::ProfileSignout::kSignoutFromCanSignInToChromeCapability,
        nil);

    // Show the age mismatch signout screen.
    if (!_ageMismatchSignoutCoordinator) {
      _ageMismatchSignoutCoordinator = [[AgeMismatchSignoutCoordinator alloc]
          initWithBaseViewController:[_sceneUIProvider activeViewController]
                             browser:self.sceneState.browserProviderInterface
                                         .mainBrowserProvider.browser];
      [_ageMismatchSignoutCoordinator start];
    }
  }
}

#pragma mark - UIBlockerManagerObserver

- (void)currentUIBlockerRemoved {
  [self fetchCapabilitiesForUnhandledIdentities];
}

#pragma mark - Internal

// Fetches capabilities for unhandled identities after building the External
// Privacy Context, which communicates device signals to the capabilities
// service.
- (void)fetchCapabilitiesForUnhandledIdentities {
  if (![self isUIAvailableToShowIOSPrompt]) {
    return;
  }

  NSArray<id<SystemIdentity>>* identities = [self unhandledIdentities];

  if (!identities.count) {
    return;
  }

  std::unique_ptr<ScopedUIBlocker> applicationUIBlocker =
      std::make_unique<ScopedUIBlocker>(self.sceneState,
                                        UIBlockerExtent::kApplication);

  __weak __typeof(self) weakSelf = self;

  // Closure to be executed after all External Privacy Contexts have been built.
  base::OnceClosure finalClosure = base::BindOnce(
      [](std::unique_ptr<ScopedUIBlocker> blocker,
         NSArray<id<SystemIdentity>>* unhandled_identities,
         __weak __typeof(self) weak_self) {
        [weak_self onAllExternalPrivacyContextsBuilt:unhandled_identities];
      },
      std::move(applicationUIBlocker), identities, weakSelf);

  base::RepeatingClosure barrierClosure =
      base::BarrierClosure(identities.count, std::move(finalClosure));

  for (id<SystemIdentity> identity in identities) {
    // Record the time at which the External Privacy Context is built.
    _handledIdentities[identity.gaiaId] = base::Time::Now();

    SystemIdentityManager::BuildExternalPrivacyContextCallback callback =
        base::BindOnce(
            [](base::RepeatingClosure closure, NSError* error) {
              // TODO(crbug.com/481654850): Add metrics.
              closure.Run();
            },
            barrierClosure);

    // Build the External Privacy Context for the given identity.
    GetApplicationContext()
        ->GetSystemIdentityManager()
        ->BuildExternalPrivacyContext(identity,
                                      [_sceneUIProvider activeViewController],
                                      std::move(callback));
  }
}

// Called after all External Privacy Contexts have been built.
- (void)onAllExternalPrivacyContextsBuilt:
    (NSArray<id<SystemIdentity>>*)identities {
  RunSystemCapabilitiesPrefetch(identities);
}

// Returns a list of identities that haven't been handled yet.
// It also removes any identities from the handled set that
// are no longer present or were fetched more than a day ago.
- (NSArray<id<SystemIdentity>>*)unhandledIdentities {
  NSArray<id<SystemIdentity>>* allIdentities =
      signin::GetIdentitiesOnDevice(self.sceneState.profileState.profile);

  std::set<GaiaId> currentGaiaIDs;
  for (id<SystemIdentity> identity in allIdentities) {
    currentGaiaIDs.insert(identity.gaiaId);
  }

  // Remove any stale identities from _handledIdentities, or those fetched
  // more than the staleness interval ago.
  std::vector<GaiaId> keys_to_remove;
  for (const auto& pair : _handledIdentities) {
    if (!currentGaiaIDs.contains(pair.first) ||
        base::Time::Now() - pair.second >=
            kExternalPrivacyContextStalenessInterval) {
      keys_to_remove.push_back(pair.first);
    }
  }

  for (const GaiaId& key : keys_to_remove) {
    _handledIdentities.erase(key);
  }

  // Return the list of identities that haven't been handled yet.
  NSMutableArray<id<SystemIdentity>>* unhandledIdentities =
      [[NSMutableArray alloc] init];
  for (id<SystemIdentity> identity in allIdentities) {
    if (!_handledIdentities.contains(identity.gaiaId)) {
      [unhandledIdentities addObject:identity];
    }
  }

  return unhandledIdentities;
}

// Whether the UI is available to show an iOS prompt.
// This is used before building External Privacy Contexts, which could
// trigger an iOS consent prompt.
- (BOOL)isUIAvailableToShowIOSPrompt {
  if (self.sceneState.profileState.initStage < ProfileInitStage::kFinal) {
    return NO;
  }

  if (self.sceneState.activationLevel < SceneActivationLevelForegroundActive) {
    return NO;
  }

  if (self.sceneState.profileState.currentUIBlocker) {
    return NO;
  }

  if (self.sceneState.signinInProgress) {
    return NO;
  }

  return YES;
}

@end

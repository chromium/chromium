// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/coordinator/signin_account_capabilities_scene_agent.h"

#import <set>

#import "base/barrier_closure.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/strings/sys_string_conversions.h"
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

@interface SigninAccountCapabilitiesSceneAgent () <
    IdentityManagerObserverBridgeDelegate,
    ProfileStateObserver,
    SystemIdentityManagerObserving,
    UIBlockerManagerObserver>
@end

@implementation SigninAccountCapabilitiesSceneAgent {
  // SceneUIProvider that provides the scene UI objects.
  __weak id<SceneUIProvider> _sceneUIProvider;

  // TODO(crbug.com/481654850): Record timestamps instead of managing the set of
  // identities.
  std::set<GaiaId> _handledIdentities;

  std::unique_ptr<SystemIdentityManagerObserverBridge>
      _systemIdentityManagerObserver;

  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserver;

  // Coordinator for the Age Mismatch prompt.
  AgeMismatchSignoutCoordinator* _ageMismatchSignoutCoordinator;
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

// Returns a list of identities that haven't been handled yet, and adds them
// to the handled set. It also removes any identities from the handled set that
// are no longer present.
- (NSArray<id<SystemIdentity>>*)unhandledIdentities {
  NSArray<id<SystemIdentity>>* allIdentities =
      signin::GetIdentitiesOnDevice(self.sceneState.profileState.profile);

  std::set<GaiaId> currentGaiaIDs;
  NSMutableArray<id<SystemIdentity>>* identities =
      [[NSMutableArray alloc] init];
  for (id<SystemIdentity> identity in allIdentities) {
    currentGaiaIDs.insert(identity.gaiaId);
    if (!_handledIdentities.contains(identity.gaiaId)) {
      [identities addObject:identity];
      _handledIdentities.insert(identity.gaiaId);
    }
  }

  // Remove any stale identities from _handledIdentities.
  for (auto it = _handledIdentities.begin(); it != _handledIdentities.end();) {
    if (!currentGaiaIDs.contains(*it)) {
      it = _handledIdentities.erase(it);
    } else {
      ++it;
    }
  }

  return identities;
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

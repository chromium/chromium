// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/age_mismatch_signout/coordinator/signin_account_capabilities_scene_agent.h"

#import <set>
#import <vector>

#import "base/barrier_closure.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/location.h"
#import "base/not_fatal_until.h"
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
#import "ios/chrome/browser/authentication/age_mismatch_signout/coordinator/age_mismatch_signout_coordinator.h"
#import "ios/chrome/browser/authentication/age_mismatch_signout/ui/age_mismatch_prompt_mode.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_utils.h"
#import "ios/chrome/browser/scoped_ui_blocker/ui_bundled/scoped_ui_blocker.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_ui_provider.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/signin_util.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"
#import "ios/chrome/browser/signin/model/system_identity_manager_observer_bridge.h"
#import "third_party/abseil-cpp/absl/container/flat_hash_set.h"

@interface SigninAccountCapabilitiesSceneAgent () <
    AgeMismatchSignoutCoordinatorDelegate,
    IdentityManagerObserverBridgeDelegate,
    ProfileStateObserver,
    SystemIdentityManagerObserving,
    UIBlockerManagerObserver>
@end

@implementation SigninAccountCapabilitiesSceneAgent {
  // SceneUIProvider that provides the scene UI objects.
  __weak id<SceneUIProvider> _sceneUIProvider;

  // The set of Gaia IDs for which the external privacy context has been built.
  absl::flat_hash_set<GaiaId, GaiaId::Hash> _handledIdentities;

  std::unique_ptr<SystemIdentityManagerObserverBridge>
      _systemIdentityManagerObserver;

  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserver;

  // Coordinator for the Age Mismatch prompt.
  AgeMismatchSignoutCoordinator* _ageMismatchSignoutCoordinator;

  // Tracks if a sign-out from an age mismatch is currently in progress.
  BOOL _isAgeMismatchSignoutInProgress;

  // Tracks if the Age Mismatch prompt has been shown at least once.
  BOOL _hasShownAgeMismatchPrompt;

  // Tracks if External Privacy Contexts are currently being built
  // asynchronously.
  BOOL _areExternalPrivacyContextsBeingBuilt;

  // The UI blocker needs to be reseted in -[SceneStateObserver
  // sceneStateDidDisableUI:] if it still exists.
  std::unique_ptr<ScopedUIBlocker> _applicationUIBlocker;
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

- (BOOL)isSignoutInProgress {
  return _isAgeMismatchSignoutInProgress || _ageMismatchSignoutCoordinator;
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
  _ageMismatchSignoutCoordinator.delegate = nil;
  [_ageMismatchSignoutCoordinator stop];
  _ageMismatchSignoutCoordinator = nil;
  _applicationUIBlocker.reset();
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
  CoreAccountInfo primaryAccountInfo =
      identityManager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  if (info.gaia != primaryAccountInfo.gaia) {
    return;
  }

  if (info.capabilities.can_sign_in_to_chrome() == signin::Tribool::kFalse) {
    // Capabilities are only available after the External Privacy Contexts have
    // been built.
    CHECK(_handledIdentities.contains(GaiaId(info.gaia)),
          base::NotFatalUntil::M155);
    [self handleAgeMismatchSignout];
  }
}

#pragma mark - UIBlockerManagerObserver

- (void)currentUIBlockerRemoved {
  [self fetchCapabilitiesForUnhandledIdentities];
}

#pragma mark - AgeMismatchSignoutCoordinatorDelegate

- (void)ageMismatchSignoutCoordinatorWantsToBeStopped:
    (AgeMismatchSignoutCoordinator*)coordinator {
  CHECK_EQ(coordinator, _ageMismatchSignoutCoordinator,
           base::NotFatalUntil::M153);
  [self stopAgeMismatchSignoutCoordinator];
}

- (void)ageMismatchSignoutCoordinatorWantsToSignIn:
    (AgeMismatchSignoutCoordinator*)coordinator {
  CHECK_EQ(coordinator, _ageMismatchSignoutCoordinator,
           base::NotFatalUntil::M153);
  // The coordinator should not be stopped while the delegate method is called,
  // to avoid reentry issues.
  __weak __typeof(self) weakSelf = self;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](__typeof(self) strong_self) {
            [strong_self closeAgeMismatchSignoutCoordinatorAndSignin];
          },
          weakSelf));
}

#pragma mark - Private

// Stops `_ageMismatchSignoutCoordinator` and start the sign-in commands.
- (void)closeAgeMismatchSignoutCoordinatorAndSignin {
  [self stopAgeMismatchSignoutCoordinator];
  // TODO(crbug.com/481654850): update the access point.
  ShowSigninCommand* command = [[ShowSigninCommand alloc]
      initWithOperation:AuthenticationOperation::kSigninOnly
            accessPoint:signin_metrics::AccessPoint::kSettings];
  [command addSigninCompletion:^(SigninCoordinator*, SigninCoordinatorResult,
                                 id<SystemIdentity>){
      // The completion is required by the API, this is a rare case where there
      // is no action to do once being signed in.
  }];
  id<SceneCommands> sceneCommandsHandler = HandlerForProtocol(
      self.sceneState.browserProviderInterface.mainBrowserProvider.browser
          ->GetCommandDispatcher(),
      SceneCommands);
  [sceneCommandsHandler showSignin:command
                baseViewController:[_sceneUIProvider activeViewController]];
}

- (void)stopAgeMismatchSignoutCoordinator {
  _ageMismatchSignoutCoordinator.delegate = nil;
  [_ageMismatchSignoutCoordinator stop];
  _ageMismatchSignoutCoordinator = nil;
}

// Fetches capabilities for unhandled identities after building the External
// Privacy Context, which communicates device signals to the capabilities
// service.
// TODO(crbug.com/484261211): Migrate these functionalities to the
// SystemIdentityManager.
- (void)fetchCapabilitiesForUnhandledIdentities {
  if (![self isUIAvailableToShowIOSPrompt]) {
    return;
  }

  NSArray<id<SystemIdentity>>* identities = [self unhandledIdentities];
  if (!identities.count) {
    return;
  }

  _areExternalPrivacyContextsBeingBuilt = YES;

  _applicationUIBlocker = std::make_unique<ScopedUIBlocker>(
      self.sceneState, UIBlockerExtent::kApplication);

  __weak __typeof(self) weakSelf = self;

  // Closure to be executed after all External Privacy Contexts have been built.
  base::OnceClosure finalClosure = base::BindOnce(
      [](__weak __typeof(self) weak_self) {
        [weak_self onAllExternalPrivacyContextsBuilt];
      },
      weakSelf);

  [self buildExternalPrivacyContextForIdentities:identities
                                           index:0
                                    finalClosure:std::move(finalClosure)];
}

// Builds External Privacy Context for the given list of identities
// sequentially. This is done recursively by iterating over the list using the
// `index` argument, ensuring only one EPC is built at a time.
- (void)buildExternalPrivacyContextForIdentities:
            (NSArray<id<SystemIdentity>>*)identities
                                           index:(NSUInteger)index
                                    finalClosure:
                                        (base::OnceClosure)finalClosure {
  if (index >= identities.count) {
    std::move(finalClosure).Run();
    return;
  }

  id<SystemIdentity> identity = identities[index];
  _handledIdentities.insert(identity.gaiaId);

  __weak __typeof(self) weakSelf = self;
  SystemIdentityManager::BuildExternalPrivacyContextCallback callback =
      base::BindOnce(
          [](id weakSelf, NSArray<id<SystemIdentity>>* identities,
             NSUInteger index, base::OnceClosure fc, NSError* error) {
            // TODO(crbug.com/481654850): Add metrics.
            [weakSelf buildExternalPrivacyContextForIdentities:identities
                                                         index:index + 1
                                                  finalClosure:std::move(fc)];
          },
          weakSelf, identities, index, std::move(finalClosure));

  // Build the External Privacy Context for the given identity.
  GetApplicationContext()
      ->GetSystemIdentityManager()
      ->BuildExternalPrivacyContext(identity,
                                    [_sceneUIProvider activeViewController],
                                    std::move(callback));
}

// Called after all External Privacy Contexts have been built.
- (void)onAllExternalPrivacyContextsBuilt {
  CHECK(_applicationUIBlocker, base::NotFatalUntil::M155);
  _applicationUIBlocker.reset();
  _areExternalPrivacyContextsBeingBuilt = NO;

  // Read capability value and signout if needed.
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(
          self.sceneState.profileState.profile);
  if (identityManager &&
      identityManager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    AccountInfo info = identityManager->FindExtendedAccountInfoByAccountId(
        identityManager->GetPrimaryAccountId(signin::ConsentLevel::kSignin));
    if (info.capabilities.can_sign_in_to_chrome() == signin::Tribool::kFalse) {
      [self handleAgeMismatchSignout];
    }
  }
}

// Signs out the user and shows the age mismatch signout UI.
- (void)handleAgeMismatchSignout {
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForProfile(
          self.sceneState.profileState.profile);
  if (!authenticationService) {
    return;
  }

  id<SystemIdentity> primaryIdentity =
      authenticationService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);

  if (primaryIdentity && !_isAgeMismatchSignoutInProgress) {
    _isAgeMismatchSignoutInProgress = YES;

    _applicationUIBlocker = std::make_unique<ScopedUIBlocker>(
        self.sceneState, UIBlockerExtent::kApplication);

    base::OnceClosure signoutCompletion = base::BindOnce(
        [](__typeof(self) strong_self, id<SystemIdentity> primary_identity) {
          [strong_self markAgeMismatchSignoutCompletedAndShowPromptForIdentity:
                           primary_identity];
        },
        self, primaryIdentity);

    authenticationService->SignOut(
        signin_metrics::ProfileSignout::kSignoutFromCanSignInToChromeCapability,
        base::CallbackToBlock(std::move(signoutCompletion)));
  }
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

  // Remove any stale identities from _handledIdentities.
  std::vector<GaiaId> keys_to_remove;
  for (const auto& key : _handledIdentities) {
    if (!currentGaiaIDs.contains(key)) {
      keys_to_remove.push_back(key);
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

- (void)markAgeMismatchSignoutCompletedAndShowPromptForIdentity:
    (id<SystemIdentity>)identity {
  CHECK(_applicationUIBlocker, base::NotFatalUntil::M155);
  _applicationUIBlocker.reset();
  _isAgeMismatchSignoutInProgress = NO;

  // Show the age mismatch signout screen.
  if (!_ageMismatchSignoutCoordinator) {
    _ageMismatchSignoutCoordinator = [[AgeMismatchSignoutCoordinator alloc]
        initWithBaseViewController:[_sceneUIProvider activeViewController]
                           browser:self.sceneState.browserProviderInterface
                                       .mainBrowserProvider.browser
                          identity:identity
                              mode:_hasShownAgeMismatchPrompt
                                       ? AgeMismatchPromptMode::kFollowUp
                                       : AgeMismatchPromptMode::kInitial];
    _ageMismatchSignoutCoordinator.delegate = self;
    [_ageMismatchSignoutCoordinator start];
    _hasShownAgeMismatchPrompt = YES;
  }
}

@end

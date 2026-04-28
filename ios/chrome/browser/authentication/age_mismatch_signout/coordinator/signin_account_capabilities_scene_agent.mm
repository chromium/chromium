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
#import "ios/chrome/browser/shared/coordinator/scene/state/scene_ui_blocker_state.h"
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

@interface SigninAccountCapabilitiesSceneAgent () <
    AgeMismatchSignoutCoordinatorDelegate,
    ExternalPrivacyContextUIProvider,
    IdentityManagerObserverBridgeDelegate,
    ProfileStateObserver,
    SceneUIBlockerStateObserver,
    UIBlockerManagerObserver>

// Called once the sign-out is done.
// `identity` is the primary identity before sign-out.
- (void)signOutDoneFromIdentity:(id<SystemIdentity>)identity;

@end

namespace {

// Called once the sign-out is done. It calls
// `-[SigninAccountCapabilitiesSceneAgent signOutDoneFromIdentity:]` to continue
// the workflow.
void SignOutDoneForSceneState(id<SystemIdentity> identity,
                              SceneState* scene_state) {
  SigninAccountCapabilitiesSceneAgent* scene_agent =
      [SigninAccountCapabilitiesSceneAgent agentFromScene:scene_state];
  [scene_agent signOutDoneFromIdentity:identity];
}

}  //  namespace

@implementation SigninAccountCapabilitiesSceneAgent {
  // SceneUIProvider that provides the scene UI objects.
  __weak id<SceneUIProvider> _sceneUIProvider;

  // UI blocker used on signout or during the Age Mismatch prompt. This needs to
  // be reseted in -[SceneStateObserver sceneStateDidDisableUI:] if it still
  // exists.
  std::unique_ptr<ScopedUIBlocker> _applicationUIBlocker;

  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserver;

  // Coordinator for the Age Mismatch prompt.
  AgeMismatchSignoutCoordinator* _ageMismatchSignoutCoordinator;

  // Tracks if a sign-out from an age mismatch is currently in progress.
  BOOL _isAgeMismatchSignoutInProgress;
}

- (instancetype)initWithSceneUIProvider:(id<SceneUIProvider>)sceneUIProvider {
  self = [super init];
  if (self) {
    _sceneUIProvider = sceneUIProvider;
    GetApplicationContext()
        ->GetSystemIdentityManager()
        ->RegisterExternalPrivacyContextProvider(self);
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
  [self.sceneState.uiBlockerState addObserver:self];

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
  [self notifyProviderReadyIfUIAvailable];
}

- (void)sceneStateDidDisableUI:(SceneState*)sceneState {
  GetApplicationContext()
      ->GetSystemIdentityManager()
      ->UnregisterExternalPrivacyContextProvider(self);
  [self.sceneState.profileState removeUIBlockerManagerObserver:self];
  [self.sceneState.profileState removeObserver:self];
  [self.sceneState.uiBlockerState removeObserver:self];
  [self.sceneState removeObserver:self];
  _identityManagerObserver.reset();
  _ageMismatchSignoutCoordinator.delegate = nil;
  [_ageMismatchSignoutCoordinator stop];
  _ageMismatchSignoutCoordinator = nil;
  _applicationUIBlocker.reset();
}

#pragma mark - SceneUIBlockerStateObserver

- (void)didHideModalOverlay {
  [self notifyProviderReadyIfUIAvailable];
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
  [self notifyProviderReadyIfUIAvailable];
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
    [self handleAgeMismatchSignout];
  }
}

#pragma mark - UIBlockerManagerObserver

- (void)currentUIBlockerRemoved {
  [self notifyProviderReadyIfUIAvailable];
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
           base::NotFatalUntil::M155);
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

#pragma mark - ExternalPrivacyContextUIProvider

- (UIViewController*)viewControllerForExternalPrivacyContext {
  if ([self isUIAvailableToShowIOSPrompt]) {
    return [_sceneUIProvider activeViewController];
  }
  return nil;
}

- (void)blockUIForExternalPrivacyContextBuild {
  CHECK([self isUIAvailableToShowIOSPrompt]);
  CHECK(!_applicationUIBlocker);
  _applicationUIBlocker = std::make_unique<ScopedUIBlocker>(
      self.sceneState, UIBlockerExtent::kApplication);
}

- (void)unblockUIOnExternalPrivacyContextBuilt {
  _applicationUIBlocker.reset();
}

#pragma mark - Private

- (void)notifyProviderReadyIfUIAvailable {
  if ([self isUIAvailableToShowIOSPrompt]) {
    GetApplicationContext()
        ->GetSystemIdentityManager()
        ->ExternalPrivacyContextProviderReady(self);
  }
}

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
  _applicationUIBlocker.reset();
  _ageMismatchSignoutCoordinator.delegate = nil;
  [_ageMismatchSignoutCoordinator stop];
  _ageMismatchSignoutCoordinator = nil;
}

// Signs out the user and shows the age mismatch signout UI.
- (void)handleAgeMismatchSignout {
  ProfileIOS* profile = self.sceneState.profileState.profile;
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForProfile(profile);
  if (!authenticationService) {
    return;
  }

  id<SystemIdentity> primaryIdentity =
      authenticationService->GetPrimaryIdentity();

  if (primaryIdentity && !_isAgeMismatchSignoutInProgress) {
    _isAgeMismatchSignoutInProgress = YES;

    _applicationUIBlocker = std::make_unique<ScopedUIBlocker>(
        self.sceneState, UIBlockerExtent::kApplication);

    signin::SignoutCompletion signoutCompletion =
        base::BindOnce(&SignOutDoneForSceneState, primaryIdentity);
    std::string sceneSessionID = self.sceneState.sceneSessionID;
    signin::MultiProfileSignOutForProfile(
        profile, sceneSessionID,
        signin_metrics::ProfileSignout::kSignoutFromCanSignInToChromeCapability,
        std::move(signoutCompletion));
  }
}

// Whether the UI is available to show an iOS prompt, which could be displayed
// when External Privacy Contexts are being built.
- (BOOL)isUIAvailableToShowIOSPrompt {
  if (self.sceneState.profileState.initStage < ProfileInitStage::kFinal) {
    return NO;
  }

  if (self.sceneState.activationLevel < SceneActivationLevelForegroundActive) {
    return NO;
  }

  if (self.isSignoutInProgress) {
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

- (void)signOutDoneFromIdentity:(id<SystemIdentity>)identity {
  if (_isAgeMismatchSignoutInProgress) {
    // This case is when there was no profile switching during sign-out.
    // This method is called on the scene agent who triggered the sign-out flow.
    CHECK(_applicationUIBlocker, base::NotFatalUntil::M155);
    // Sign-out is done.
    _isAgeMismatchSignoutInProgress = NO;
  } else {
    // This case is when there was a profile switching during sign-out.
    // This method is called on the newly created scene agent. This scene agent
    // didn't trigger the sign-out. But it needs to finish the workflow.
    CHECK(!_applicationUIBlocker, base::NotFatalUntil::M155);
    _applicationUIBlocker = std::make_unique<ScopedUIBlocker>(
        self.sceneState, UIBlockerExtent::kApplication);
  }
  CHECK(!_ageMismatchSignoutCoordinator, base::NotFatalUntil::M155);
  // Show the age mismatch signout screen.
  _ageMismatchSignoutCoordinator = [[AgeMismatchSignoutCoordinator alloc]
      initWithBaseViewController:[_sceneUIProvider activeViewController]
                         browser:self.sceneState.browserProviderInterface
                                     .mainBrowserProvider.browser
                        identity:identity
                            mode:AgeMismatchPromptMode::kStandard];
  _ageMismatchSignoutCoordinator.delegate = self;
  [_ageMismatchSignoutCoordinator start];
}

@end

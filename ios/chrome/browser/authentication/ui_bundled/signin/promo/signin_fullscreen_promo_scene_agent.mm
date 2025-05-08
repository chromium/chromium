// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/signin/promo/signin_fullscreen_promo_scene_agent.h"

#import "base/memory/raw_ptr.h"
#import "base/notreached.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "components/sync/service/sync_service.h"
#import "components/version_info/version_info.h"
#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/profile/profile_state_observer.h"
#import "ios/chrome/browser/authentication/ui_bundled/history_sync/history_sync_utils.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_utils.h"
#import "ios/chrome/browser/promos_manager/model/constants.h"
#import "ios/chrome/browser/promos_manager/model/features.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"

@interface SigninFullscreenPromoSceneAgent () <
    ProfileStateObserver,
    IdentityManagerObserverBridgeDelegate>
@end

@implementation SigninFullscreenPromoSceneAgent {
  // The PromosManager used by the scene agent to manage promo display.
  raw_ptr<PromosManager> _promosManager;

  // The AuthenticationService used by the mediator to monitor sign-in status.
  raw_ptr<AuthenticationService> _authService;

  // Bridge to observe changes in the identity manager.
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserver;

  // The SyncService used to check if the history sync can be skipped.
  raw_ptr<syncer::SyncService> _syncService;

  // The PrefService used to check if the history sync can be skipped.
  raw_ptr<PrefService> _prefService;
}

- (instancetype)initWithPromosManager:(PromosManager*)promosManager
                          authService:(AuthenticationService*)authService
                      identityManager:(signin::IdentityManager*)identityManager
                          syncService:(syncer::SyncService*)syncService
                          prefService:(PrefService*)prefService {
  self = [super init];
  if (self) {
    _promosManager = promosManager;
    _authService = authService;
    _identityManagerObserver =
        std::make_unique<signin::IdentityManagerObserverBridge>(identityManager,
                                                                self);
    _syncService = syncService;
    _prefService = prefService;
  }
  return self;
}

#pragma mark - ObservingSceneAgent

- (void)setSceneState:(SceneState*)sceneState {
  [super setSceneState:sceneState];

  [self.sceneState.profileState addObserver:self];
}

#pragma mark - ProfileStateObserver

- (void)profileState:(ProfileState*)profileState
    didTransitionToInitStage:(ProfileInitStage)nextInitStage
               fromInitStage:(ProfileInitStage)fromInitStage {
  [self handlePromoRegistration];
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  [self handlePromoRegistration];
}

- (void)sceneStateDidDisableUI:(SceneState*)sceneState {
  _identityManagerObserver.reset();
  _identityManagerObserver = nullptr;
  _promosManager = nullptr;
  _authService = nullptr;
  _syncService = nullptr;
  _prefService = nullptr;
  [self.sceneState.profileState removeObserver:self];
  [self.sceneState removeObserver:self];
}

#pragma mark - Private

// Registers or deregisters the sign-in fullscreen promo if the profile
// initialization is over and the scene is in the foreground.
- (void)handlePromoRegistration {
  // Check that the profile initialization is over (the stage
  // ProfileInitStage::kFinal is reached).
  // added
  if (self.sceneState.profileState.initStage < ProfileInitStage::kFinal) {
    return;
  }

  //  Check that the scene is in the foreground.
  if (self.sceneState.activationLevel < SceneActivationLevelForegroundActive) {
    return;
  }

  if (!self.sceneState.profileState.currentUIBlocker &&
      signin::ShouldPresentUserSigninUpgrade(
          self.sceneState.browserProviderInterface.mainBrowserProvider.browser
              ->GetProfile(),
          version_info::GetVersion())) {
    _promosManager->RegisterPromoForContinuousDisplay(
        promos_manager::Promo::SigninFullscreen);
    return;
  }

  _promosManager->DeregisterPromo(promos_manager::Promo::SigninFullscreen);
}

#pragma mark - IdentityManagerObserverBridgeDelegate

- (void)onPrimaryAccountChanged:
    (const signin::PrimaryAccountChangeEvent&)event {
  if (_authService->HasPrimaryIdentity(signin::ConsentLevel::kSignin)) {
    history_sync::HistorySyncSkipReason skipReason =
        history_sync::GetSkipReason(_syncService, _authService, _prefService,
                                    /*isOptional=*/YES);
    if (skipReason != history_sync::HistorySyncSkipReason::kNone) {
      // Deregister the promo if the user signed in and the history sync can be
      // skipped.
      _promosManager->DeregisterPromo(promos_manager::Promo::SigninFullscreen);
    }
  }
}

- (void)onIdentityManagerShutdown:(signin::IdentityManager*)identityManager {
  NOTREACHED(base::NotFatalUntil::M142);
}

@end

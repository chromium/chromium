// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/signin/promo/signin_fullscreen_promo_scene_agent.h"

#import "base/memory/raw_ptr.h"
#import "components/version_info/version_info.h"
#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/profile/profile_state_observer.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_utils.h"
#import "ios/chrome/browser/promos_manager/model/constants.h"
#import "ios/chrome/browser/promos_manager/model/features.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

@interface SigninFullscreenPromoSceneAgent () <ProfileStateObserver>
@end

@implementation SigninFullscreenPromoSceneAgent {
  raw_ptr<PromosManager> _promosManager;
}

- (instancetype)initWithPromosManager:(PromosManager*)promosManager {
  self = [super init];
  if (self) {
    _promosManager = promosManager;
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

@end

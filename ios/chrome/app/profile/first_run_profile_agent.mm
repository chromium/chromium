// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/profile/first_run_profile_agent.h"

#import "base/check.h"
#import "base/check_op.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/startup_information.h"
#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_coordinator.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_screen_provider.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_observer.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/signin_util.h"
#import "ios/chrome/browser/ui/scoped_ui_blocker/scoped_ui_blocker.h"

@interface FirstRunProfileAgent () <FirstRunCoordinatorDelegate,
                                    SceneStateObserver>

@end

@implementation FirstRunProfileAgent {
  // UI blocker used while the FRE UI is shown in the scene controlled by this
  // object.
  std::unique_ptr<ScopedUIBlocker> _firstRunUIBlocker;

  // The scene that is chosen for presenting the FRE on.
  SceneState* _presentingSceneState;

  // Coordinator of the First Run UI.
  FirstRunCoordinator* _firstRunCoordinator;
}

#pragma mark - SceneStateObserver

- (void)sceneStateDidDisableUI:(SceneState*)sceneState {
  _firstRunUIBlocker.reset();

  [_firstRunCoordinator stop];
  _firstRunCoordinator = nil;

  [sceneState removeObserver:self];
  _presentingSceneState = nil;
}

#pragma mark - ProfileStateObserver

- (void)profileState:(ProfileState*)profileState
    didTransitionToInitStage:(ProfileInitStage)nextInitStage
               fromInitStage:(ProfileInitStage)fromInitStage {
  if (nextInitStage == ProfileInitStage::kFirstRun) {
    [self handleFirstRunStage];
    return;
  }

  if (fromInitStage == ProfileInitStage::kFirstRun) {
    [self.profileState removeAgent:self];
    return;
  }
}

- (void)profileState:(ProfileState*)profileState
    firstSceneHasInitializedUI:(SceneState*)sceneState {
  // Select the first scene that the app declares as initialized to present
  // the FRE UI on.
  _presentingSceneState = sceneState;
  [_presentingSceneState addObserver:self];

  if (self.profileState.initStage != ProfileInitStage::kFirstRun) {
    return;
  }

  // Skip the FRE because it wasn't determined to be needed.
  if (!self.profileState.appState.startupInformation.isFirstRun) {
    return;
  }

  [self showFirstRunUI];
}

#pragma mark - Private methods

- (void)handleFirstRunStage {
  // Skip the FRE because it wasn't determined to be needed.
  if (!self.profileState.appState.startupInformation.isFirstRun) {
    [self.profileState queueTransitionToNextInitStage];
    return;
  }

  // Cannot show the FRE UI immediately because there is no scene state to
  // present from.
  if (!_presentingSceneState) {
    return;
  }

  [self showFirstRunUI];
}

- (void)showFirstRunUI {
  DCHECK_EQ(self.profileState.initStage, ProfileInitStage::kFirstRun);

  // There must be a designated presenting scene before showing the first run
  // UI.
  DCHECK(_presentingSceneState);

  id<BrowserProvider> presentingInterface =
      _presentingSceneState.browserProviderInterface.currentBrowserProvider;
  Browser* browser = presentingInterface.browser;
  ProfileIOS* profile = browser->GetProfile()->GetOriginalProfile();

  DCHECK(!_firstRunUIBlocker);
  _firstRunUIBlocker = std::make_unique<ScopedUIBlocker>(_presentingSceneState);

  // TODO(crbug.com/343699504): Remove pre-fetching capabilities once these are
  // loaded in iSL.
  RunSystemCapabilitiesPrefetch(
      ChromeAccountManagerServiceFactory::GetForProfile(profile)
          ->GetAllIdentities());

  FirstRunScreenProvider* provider =
      [[FirstRunScreenProvider alloc] initForProfile:profile];

  _firstRunCoordinator = [[FirstRunCoordinator alloc]
      initWithBaseViewController:presentingInterface.viewController
                         browser:browser
                  screenProvider:provider];
  _firstRunCoordinator.delegate = self;
  [_firstRunCoordinator start];
}

#pragma mark - FirstRunCoordinatorDelegate

- (void)didFinishFirstRun {
  DCHECK_EQ(self.profileState.initStage, ProfileInitStage::kFirstRun);
  _firstRunUIBlocker.reset();
  [_firstRunCoordinator stop];
  _firstRunCoordinator = nil;
  [self.profileState queueTransitionToNextInitStage];
}

@end

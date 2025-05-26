// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/profile/first_run_profile_agent.h"

#import "base/check.h"
#import "base/check_op.h"
#import "components/feature_engagement/public/tracker.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/startup_information.h"
#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_utils.h"
#import "ios/chrome/browser/device_orientation/ui_bundled/scoped_force_portrait_orientation.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_coordinator.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_screen_provider.h"
#import "ios/chrome/browser/first_run/ui_bundled/guided_tour/guided_tour_coordinator.h"
#import "ios/chrome/browser/first_run/ui_bundled/guided_tour/guided_tour_promo_coordinator.h"
#import "ios/chrome/browser/scoped_ui_blocker/ui_bundled/scoped_ui_blocker.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_observer.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/guided_tour_commands.h"
#import "ios/chrome/browser/shared/public/commands/tab_grid_commands.h"
#import "ios/chrome/browser/shared/public/commands/tab_grid_toolbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/signin_util.h"

@interface FirstRunProfileAgent () <FirstRunCoordinatorDelegate,
                                    GuidedTourCoordinatorDelegate,
                                    GuidedTourPromoCoordinatorDelegate,
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

  // Coordinator for the Guided Tour Promo.
  GuidedTourPromoCoordinator* _guidedTourPromoCoordinator;

  // Coordinator for the first step of the guided tour.
  GuidedTourCoordinator* _guidedTourCoordinator;

  // The current step in the guided tour.
  GuidedTourStep _currentGuidedTourStep;

  // Used to force the device orientation in portrait mode on iPhone.
  std::unique_ptr<ScopedForcePortraitOrientation> _scopedForceOrientation;

  // Used to prevent other IPHs from showing during the Guided Tour.
  std::unique_ptr<feature_engagement::DisplayLockHandle> _displayLock;
}

#pragma mark - Public

- (void)tabGridWasPresented {
  if (_currentGuidedTourStep == GuidedTourStepTabGridIncognito) {
    id<BrowserProvider> presentingInterface =
        _presentingSceneState.browserProviderInterface.currentBrowserProvider;
    Browser* browser = presentingInterface.browser;
    __weak FirstRunProfileAgent* weakSelf = self;
    ProceduralBlock completion = ^{
      [weakSelf showLongPressStep];
    };
    id<TabGridToolbarCommands> handler = HandlerForProtocol(
        browser->GetCommandDispatcher(), TabGridToolbarCommands);
    [handler showGuidedTourIncognitoStepWithDismissalCompletion:completion];
  }
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
    willTransitionToInitStage:(ProfileInitStage)nextInitStage
                fromInitStage:(ProfileInitStage)fromInitStage {
  if (nextInitStage != ProfileInitStage::kFirstRun) {
    return;
  }

  AppState* appState = profileState.appState;
  if (appState.startupInformation.isFirstRun) {
    _scopedForceOrientation = ForcePortraitOrientationOnIphone(appState);
    if (IsBestOfAppFREEnabled()) {
      id<BrowserProvider> presentingInterface =
          _presentingSceneState.browserProviderInterface.currentBrowserProvider;
      Browser* browser = presentingInterface.browser;
      feature_engagement::Tracker* engagementTracker =
          feature_engagement::TrackerFactory::GetForProfile(
              browser->GetProfile()->GetOriginalProfile());
      _displayLock = engagementTracker->AcquireDisplayLock();
    }
  }
}

- (void)profileState:(ProfileState*)profileState
    didTransitionToInitStage:(ProfileInitStage)nextInitStage
               fromInitStage:(ProfileInitStage)fromInitStage {
  if (nextInitStage == ProfileInitStage::kFirstRun) {
    [self handleFirstRunStage];
    return;
  }

  if (fromInitStage == ProfileInitStage::kFirstRun) {
    if (!IsBestOfAppGuidedTourEnabled()) {
      _scopedForceOrientation.reset();
      [profileState removeAgent:self];
      return;
    }
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

  // The UI will be blocked until the user completes the first run flow, so
  // inform the ProfileState this is going to happen.
  [self.profileState willBlockProfileInitialisationForUI];

  id<BrowserProvider> presentingInterface =
      _presentingSceneState.browserProviderInterface.currentBrowserProvider;
  Browser* browser = presentingInterface.browser;
  ProfileIOS* profile = browser->GetProfile()->GetOriginalProfile();

  DCHECK(!_firstRunUIBlocker);
  _firstRunUIBlocker = std::make_unique<ScopedUIBlocker>(_presentingSceneState);

  // TODO(crbug.com/343699504): Remove pre-fetching capabilities once these are
  // loaded in iSL.
  RunSystemCapabilitiesPrefetch(signin::GetIdentitiesOnDevice(profile));

  FirstRunScreenProvider* provider =
      [[FirstRunScreenProvider alloc] initForProfile:profile];

  _firstRunCoordinator = [[FirstRunCoordinator alloc]
      initWithBaseViewController:presentingInterface.viewController
                         browser:browser
                  screenProvider:provider];
  _firstRunCoordinator.delegate = self;
  [_firstRunCoordinator start];
}

- (void)showGuidedTourPrompt {
  if (_guidedTourPromoCoordinator) {
    return;
  }
  id<BrowserProvider> presentingInterface =
      _presentingSceneState.browserProviderInterface.currentBrowserProvider;
  Browser* browser = presentingInterface.browser;
  _guidedTourPromoCoordinator = [[GuidedTourPromoCoordinator alloc]
      initWithBaseViewController:presentingInterface.viewController
                         browser:browser];
  _guidedTourPromoCoordinator.delegate = self;
  [_guidedTourPromoCoordinator start];
}

- (void)showNTPStep {
  _currentGuidedTourStep = GuidedTourStepNTP;
  // Command Dispatcher to show NTP IPH
  id<BrowserProvider> presentingInterface =
      _presentingSceneState.browserProviderInterface.currentBrowserProvider;
  Browser* browser = presentingInterface.browser;
  id<GuidedTourCommands> handler =
      HandlerForProtocol(browser->GetCommandDispatcher(), GuidedTourCommands);
  [handler highlightViewInStep:GuidedTourStepNTP];

  _guidedTourCoordinator = [[GuidedTourCoordinator alloc]
            initWithStep:GuidedTourStepNTP
      baseViewController:presentingInterface.viewController
                 browser:browser
                delegate:self];
  [_guidedTourCoordinator start];
}

- (void)showLongPressStep {
  _currentGuidedTourStep = GuidedTourStepTabGridLongPress;
  id<BrowserProvider> presentingInterface =
      _presentingSceneState.browserProviderInterface.currentBrowserProvider;
  Browser* browser = presentingInterface.browser;
  __weak FirstRunProfileAgent* weakSelf = self;
  ProceduralBlock completion = ^{
    [weakSelf showTabGroupStep];
  };
  id<TabGridCommands> handler =
      HandlerForProtocol(browser->GetCommandDispatcher(), TabGridCommands);
  [handler showGuidedTourLongPressStepWithDismissalCompletion:completion];
}

- (void)showTabGroupStep {
  _currentGuidedTourStep = GuidedTourStepTabGridTabGroup;
  id<BrowserProvider> presentingInterface =
      _presentingSceneState.browserProviderInterface.currentBrowserProvider;
  Browser* browser = presentingInterface.browser;
  __weak FirstRunProfileAgent* weakSelf = self;
  ProceduralBlock completion = ^{
    [weakSelf guidedTourCompleted];
  };
  id<TabGridToolbarCommands> handler = HandlerForProtocol(
      browser->GetCommandDispatcher(), TabGridToolbarCommands);
  [handler showGuidedTourTabGroupStepWithDismissalCompletion:completion];
}

- (void)guidedTourCompleted {
  _displayLock.reset();
  _scopedForceOrientation.reset();
  [self.profileState removeAgent:self];
}

#pragma mark - GuidedTourCoordinatorDelegate

- (void)stepCompleted:(GuidedTourStep)step {
  CHECK_EQ(step, _currentGuidedTourStep);
  if (step == GuidedTourStepNTP) {
    _currentGuidedTourStep = GuidedTourStepTabGridIncognito;
    id<BrowserProvider> presentingInterface =
        _presentingSceneState.browserProviderInterface.currentBrowserProvider;
    Browser* browser = presentingInterface.browser;
    id<ApplicationCommands> applicationHandler = HandlerForProtocol(
        browser->GetCommandDispatcher(), ApplicationCommands);
    [applicationHandler displayTabGridInMode:TabGridOpeningMode::kRegular];
  }
}

- (void)nextTappedForStep:(GuidedTourStep)step {
  if (step == GuidedTourStepNTP) {
    id<BrowserProvider> presentingInterface =
        _presentingSceneState.browserProviderInterface.currentBrowserProvider;
    Browser* browser = presentingInterface.browser;
    id<GuidedTourCommands> handler =
        HandlerForProtocol(browser->GetCommandDispatcher(), GuidedTourCommands);
    [handler stepCompleted:GuidedTourStepNTP];
  }
}

#pragma mark - GuidedTourPromoCoordinatorDelegate

- (void)dismissGuidedTourPromo {
  [_guidedTourPromoCoordinator stopWithCompletion:nil];
  [self guidedTourCompleted];
}

- (void)startGuidedTour {
  __weak FirstRunProfileAgent* weakSelf = self;
  ProceduralBlock completion = ^{
    [weakSelf showNTPStep];
  };
  [_guidedTourPromoCoordinator stopWithCompletion:completion];
}

#pragma mark - FirstRunCoordinatorDelegate

- (void)didFinishFirstRun {
  DCHECK_EQ(self.profileState.initStage, ProfileInitStage::kFirstRun);
  _firstRunUIBlocker.reset();
  ProceduralBlock completion;
  if (IsBestOfAppGuidedTourEnabled()) {
    __weak FirstRunProfileAgent* weakSelf = self;
    completion = ^{
      [weakSelf showGuidedTourPrompt];
    };
  }
  [_firstRunCoordinator stopWithCompletion:completion];
  _firstRunCoordinator = nil;
  [self.profileState queueTransitionToNextInitStage];
}

@end

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/profile/search_engine_choice_profile_agent.h"

#import <memory>

#import "base/check.h"
#import "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/device_orientation/ui_bundled/scoped_force_portrait_orientation.h"
#import "ios/chrome/browser/scoped_ui_blocker/ui_bundled/scoped_ui_blocker.h"
#import "ios/chrome/browser/search_engine_choice/coordinator/search_engine_choice_coordinator.h"
#import "ios/chrome/browser/search_engine_choice/model/search_engine_choice_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_observer.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/signin/model/signin_util.h"

namespace {

// Enum storing the result of deciding whether the Search Engine Choice
// Screen should be skipped or not.
enum class SkipScreenDecision {
  kUnknown,
  kPresent,
  kSkip,
};

}  // namespace

@interface SearchEngineChoiceProfileAgent () <
    SearchEngineChoiceCoordinatorDelegate>
@end

@implementation SearchEngineChoiceProfileAgent {
  // The coordinator of the search engine choice screen.
  SearchEngineChoiceCoordinator* _searchEngineChoiceCoordinator;
  // UI blocker used by the search engine selection screen.
  std::unique_ptr<ScopedUIBlocker> _searchEngineChoiceUIBlocker;
  // Scene state ID where the search engine choice dialog is displayed.
  std::string _searchEngineChoiceSceneStateID;
  // Store whether the Search Engine Choice Screen should be skipped or not.
  SkipScreenDecision _skipScreenDecision;
  // Used to force the device orientation in portrait mode on iPhone.
  std::unique_ptr<ScopedForcePortraitOrientation> _scopedForceOrientation;
}

#pragma mark - SceneObservingProfileAgent

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  if (self.profileState.initStage != ProfileInitStage::kChoiceScreen) {
    return;
  }

  switch (level) {
    case SceneActivationLevelBackground:
    case SceneActivationLevelForegroundInactive:
      // Nothing to do as the SceneState is not ready.
      break;

    case SceneActivationLevelForegroundActive:
      [self maybeShowChoiceScreen:sceneState];
      break;

    case SceneActivationLevelDisconnected:
    case SceneActivationLevelUnattached:
      [self sceneStateDisconnected:sceneState];
      break;
  }
}

#pragma mark - ProfileStateObserver

- (void)profileState:(ProfileState*)profileState
    willTransitionToInitStage:(ProfileInitStage)nextInitStage
                fromInitStage:(ProfileInitStage)fromInitStage {
  if (nextInitStage != ProfileInitStage::kChoiceScreen) {
    return;
  }
  if ([self shouldShowChoiceScreen]) {
    AppState* appState = profileState.appState;
    _scopedForceOrientation = ForcePortraitOrientationOnIphone(appState);
  }
}

- (void)profileState:(ProfileState*)profileState
    didTransitionToInitStage:(ProfileInitStage)nextInitStage
               fromInitStage:(ProfileInitStage)fromInitStage {
  if (nextInitStage == ProfileInitStage::kChoiceScreen) {
    // Try to present the Choice Screen on the first active SceneState.
    if (SceneState* sceneState = profileState.foregroundActiveScene) {
      [self maybeShowChoiceScreen:sceneState];
    }
    return;
  }

  if (fromInitStage == ProfileInitStage::kChoiceScreen) {
    _scopedForceOrientation.reset();
    [profileState removeAgent:self];
  }
}

#pragma mark - SearchEngineChoiceCoordinatorDelegate

- (void)choiceScreenWasDismissed:(SearchEngineChoiceCoordinator*)coordinator {
  DCHECK_EQ(_searchEngineChoiceCoordinator, coordinator);
  [self stopPresentingChoiceScreen];

  // Advance to the next stage when the screen is dismissed by the user.
  if (self.profileState.initStage == ProfileInitStage::kChoiceScreen) {
    [self.profileState queueTransitionToNextInitStage];
  }
}

#pragma mark - Private

// Returns whether the app was started via an external intent (i.e. any
// connected scene was given an external intent).
- (BOOL)startupHadExternalIntent {
  for (SceneState* sceneState in self.profileState.connectedScenes) {
    if (sceneState.startupHadExternalIntent) {
      return YES;
    }
  }

  return NO;
}

// Returns whether the choice screen should be presented or not. The return
// value is cached to ensure stability.
- (BOOL)shouldShowChoiceScreen {
  DCHECK(self.profileState.profile);
  ProfileIOS* profile = self.profileState.profile;

  if (_skipScreenDecision == SkipScreenDecision::kUnknown) {
    if (ShouldDisplaySearchEngineChoiceScreen(
            *profile, /*is_first_run_entrypoint=*/false,
            [self startupHadExternalIntent])) {
      _skipScreenDecision = SkipScreenDecision::kPresent;
    } else {
      _skipScreenDecision = SkipScreenDecision::kSkip;
    }
  }

  return _skipScreenDecision == SkipScreenDecision::kPresent;
}

// Tries to present the choice screen on `sceneState`. If the screen is not
// presented for any reason, then advance the application init state.
- (void)maybeShowChoiceScreen:(SceneState*)sceneState {
  DCHECK_EQ(self.profileState.initStage, ProfileInitStage::kChoiceScreen);
  DCHECK_EQ(sceneState.activationLevel, SceneActivationLevelForegroundActive);

  // If the Choice Screen is already presented on another SceneState, then
  // there is nothing to do.
  if (_searchEngineChoiceCoordinator) {
    DCHECK(_searchEngineChoiceUIBlocker);
    DCHECK(!_searchEngineChoiceSceneStateID.empty());
    return;
  }

  DCHECK(!_searchEngineChoiceUIBlocker);
  DCHECK(_searchEngineChoiceSceneStateID.empty());

  if (![self shouldShowChoiceScreen]) {
    // If there is no need to present the screen, then transition to the next
    // application stage (otherwise the transition will happen once the user
    // has selected a default search engine and completed the workflow). In
    // that case, the method won't be called again.
    [self.profileState queueTransitionToNextInitStage];
    return;
  }

  // Present the screen.
  _searchEngineChoiceSceneStateID = sceneState.sceneSessionID;
  _searchEngineChoiceUIBlocker = std::make_unique<ScopedUIBlocker>(sceneState);

  id<BrowserProvider> browserProvider =
      sceneState.browserProviderInterface.currentBrowserProvider;

  _searchEngineChoiceCoordinator = [[SearchEngineChoiceCoordinator alloc]
      initWithBaseViewController:browserProvider.viewController
                         browser:browserProvider.browser];
  _searchEngineChoiceCoordinator.delegate = self;
  [_searchEngineChoiceCoordinator start];
}

// Tries to dismiss the choice screen if presented by `sceneState` as the
// SceneState will be disconnected or detached soon. If that `sceneState`
// was presenting the Search Engine Choice Screen, move the presentation
// to the next active SceneState, if any.
- (void)sceneStateDisconnected:(SceneState*)sceneState {
  DCHECK_EQ(self.profileState.initStage, ProfileInitStage::kChoiceScreen);
  if (!_searchEngineChoiceCoordinator) {
    // Nothing to do if the Search Engine Choice Screen is not presented.
    return;
  }

  DCHECK(_searchEngineChoiceUIBlocker);
  DCHECK(!_searchEngineChoiceSceneStateID.empty());

  if (_searchEngineChoiceSceneStateID != sceneState.sceneSessionID) {
    // Nothing to do if the Search Engine Choice Screen is not presented
    // by `sceneState`.
    return;
  }

  [self stopPresentingChoiceScreen];
  if (SceneState* nextSceneState = self.profileState.foregroundActiveScene) {
    [self maybeShowChoiceScreen:nextSceneState];
  }
}

// Stops presenting the choice screen. Called after it has been dismissed
// by the user or when programmatically dismissing when a SceneState is
// detached or disconnected while the screen is presented.
- (void)stopPresentingChoiceScreen {
  DCHECK(!_searchEngineChoiceSceneStateID.empty());
  _searchEngineChoiceSceneStateID.clear();
  _searchEngineChoiceUIBlocker.reset();

  _searchEngineChoiceCoordinator.delegate = nil;
  [_searchEngineChoiceCoordinator stop];
  _searchEngineChoiceCoordinator = nil;
}

@end

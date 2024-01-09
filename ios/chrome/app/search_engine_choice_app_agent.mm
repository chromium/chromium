// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/search_engine_choice_app_agent.h"

#import <memory>

#import "base/check.h"
#import "components/search_engines/search_engine_choice_utils.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/policy/model/browser_state_policy_connector.h"
#import "ios/chrome/browser/promos_manager/promos_manager.h"
#import "ios/chrome/browser/promos_manager/promos_manager_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_observer.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/ui/scoped_ui_blocker/scoped_ui_blocker.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_coordinator.h"
#import "ios/public/provider/chrome/browser/signin/choice_api.h"

namespace {
bool IsChoiceEnabledInNormalRun() {
  if (experimental_flags::AlwaysDisplaySearchEngineChoice()) {
    // This branch is only selected in tests that are related to choice screen.
    return true;
  }
  if (tests_hook::DisableDefaultSearchEngineChoice()) {
    // This branch is taken in every other tests.
    return false;
  }
  if (ios::provider::DisableDefaultSearchEngineChoice()) {
    // Outside of tests, this view should be disabled upstream.
    return false;
  }
  return search_engines::IsChoiceScreenFlagEnabled(
      search_engines::ChoicePromo::kDialog);
}
}  // namespace

@interface SearchEngineChoiceAppAgent () <SearchEngineChoiceCoordinatorDelegate>
@end

@implementation SearchEngineChoiceAppAgent {
  // The coordinator of the search engine choice screen.
  SearchEngineChoiceCoordinator* _searchEngineChoiceCoordinator;
  // UI blocker used by the search engine selection screen.
  std::unique_ptr<ScopedUIBlocker> _searchEngineChoiceUIBlocker;
}

#pragma mark - SceneObservingAppAgent

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  // Ignore SceneState activation if the app is not yet ready.
  if (self.appState.initStage > InitStageFirstRun) {
    switch (level) {
      case SceneActivationLevelForegroundInactive:
        break;
      case SceneActivationLevelForegroundActive:
        [self maybeShowChoiceScreen:sceneState];
        break;
      case SceneActivationLevelBackground:
      case SceneActivationLevelDisconnected:
      case SceneActivationLevelUnattached:
        [self choiceScreenWillBeDismissed:_searchEngineChoiceCoordinator];
        break;
    }
  }

  [super sceneState:sceneState transitionedToActivationLevel:level];
}

#pragma mark - AppStateObserver

- (void)appState:(AppState*)appState
    didTransitionFromInitStage:(InitStage)previousInitStage {
  // Instantiate the `SearchEngineChoiceCoordinator` if necessary if any
  // SceneState reached the `SceneActivationLevelForegroundActive` level
  // before the app was ready.
  if (self.appState.initStage > InitStageFirstRun) {
    for (SceneState* sceneState in self.appState.connectedScenes) {
      if (sceneState.activationLevel == SceneActivationLevelForegroundActive) {
        [self maybeShowChoiceScreen:sceneState];
      }
    }
  }
  [super appState:appState didTransitionFromInitStage:previousInitStage];
}

#pragma mark - SearchEngineChoiceCoordinatorDelegate

- (void)choiceScreenWillBeDismissed:
    (SearchEngineChoiceCoordinator*)coordinator {
  DCHECK_EQ(_searchEngineChoiceCoordinator, coordinator);
  _searchEngineChoiceUIBlocker.reset();
  [_searchEngineChoiceCoordinator stop];
  _searchEngineChoiceCoordinator = nil;
}

#pragma mark - Private

- (void)maybeShowChoiceScreen:(SceneState*)sceneState {
  // The application needs to be ready (i.e. the Browser created, ...) before
  // the choice screen can be presented. Assert this is the case.
  DCHECK_GT(self.appState.initStage, InitStageFirstRun);
  if (_searchEngineChoiceCoordinator) {
    return;
  }
  if ([self shouldShowChoiceScreen:sceneState]) {
    DCHECK(!_searchEngineChoiceUIBlocker);
    _searchEngineChoiceUIBlocker =
        std::make_unique<ScopedUIBlocker>(sceneState);
    _searchEngineChoiceCoordinator = [[SearchEngineChoiceCoordinator alloc]
        initWithBaseViewController:sceneState.browserProviderInterface
                                       .currentBrowserProvider.viewController
                           browser:sceneState.browserProviderInterface
                                       .currentBrowserProvider.browser];
    _searchEngineChoiceCoordinator.delegate = self;
    [_searchEngineChoiceCoordinator start];
  }
}

- (BOOL)shouldShowChoiceScreen:(SceneState*)sceneState {
  if (!IsChoiceEnabledInNormalRun()) {
    return NO;
  }
  ChromeBrowserState* browserState =
      sceneState.browserProviderInterface.mainBrowserProvider.browser
          ->GetBrowserState();
  if (!browserState) {
    return NO;
  }
  BrowserStatePolicyConnector* policyConnector =
      browserState->GetPolicyConnector();
  return search_engines::ShouldShowChoiceScreen(
      *policyConnector->GetPolicyService(),
      /*profile_properties=*/
      {.is_regular_profile = true, .pref_service = browserState->GetPrefs()},
      ios::TemplateURLServiceFactory::GetForBrowserState(browserState));
}

@end

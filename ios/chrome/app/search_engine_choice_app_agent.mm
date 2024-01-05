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
  if (!search_engines::IsChoiceScreenFlagEnabled(
          search_engines::ChoicePromo::kDialog)) {
    // Outside of tests, do not show the view if flags disable it.
    return false;
  }
  return ios::provider::IsChoiceEnabled();
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
  switch (level) {
    case SceneActivationLevelForegroundInactive:
      break;
    case SceneActivationLevelForegroundActive:
      [self maybeShowChoiceSceen:sceneState];
      break;
    case SceneActivationLevelBackground:
    case SceneActivationLevelDisconnected:
    case SceneActivationLevelUnattached:
      [self choiceScreenWillBeDismissed:_searchEngineChoiceCoordinator];
      break;
  }
  [super sceneState:sceneState transitionedToActivationLevel:level];
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

- (void)maybeShowChoiceSceen:(SceneState*)sceneState {
  if (_searchEngineChoiceCoordinator) {
    return;
  }
  if ([self shouldShowChoiceScreen]) {
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

- (BOOL)shouldShowChoiceScreen {
  if (!IsChoiceEnabledInNormalRun()) {
    return NO;
  }
  if (self.appState.initStage == InitStageFirstRun) {
    return NO;
  }
  ChromeBrowserState* browserState = self.appState.mainBrowserState;
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

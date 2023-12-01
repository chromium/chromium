// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/search_engine_choice_app_agent.h"

#import <memory>

#import "base/check.h"
#import "components/search_engines/search_engine_choice_utils.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/policy/browser_state_policy_connector.h"
#import "ios/chrome/browser/promos_manager/promos_manager.h"
#import "ios/chrome/browser/promos_manager/promos_manager_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_observer.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/scoped_ui_blocker/scoped_ui_blocker.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_coordinator.h"
#import "ios/public/provider/chrome/browser/signin/choice_api.h"

@interface SearchEngineChoiceAppAgent () <SearchEngineChoiceCoordinatorDelegate>
@end

@implementation SearchEngineChoiceAppAgent {
  SearchEngineChoiceCoordinator* _searchEngineChoiceCoordinator;
  // TODO(b/306576460): Use a ScopedUIBlocker to bloack other windows on iPads.
}

#pragma mark - SceneObservingAppAgent

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  switch (level) {
    case SceneActivationLevelForegroundActive:
    case SceneActivationLevelForegroundInactive: {
      [self maybeShowChoiceSceen:sceneState];
      break;
    }
    case SceneActivationLevelBackground: {
      [self choiceScreenWillBeDismissed:_searchEngineChoiceCoordinator];
      break;
    }
    case SceneActivationLevelDisconnected:
    case SceneActivationLevelUnattached: {
      break;
    }
  }
  [super sceneState:sceneState transitionedToActivationLevel:level];
}

#pragma mark - SearchEngineChoiceCoordinatorDelegate

- (void)choiceScreenWillBeDismissed:
    (SearchEngineChoiceCoordinator*)coordinator {
  [coordinator stop];
  coordinator = nil;
}

#pragma mark - Private

- (void)maybeShowChoiceSceen:(SceneState*)sceneState {
  if (_searchEngineChoiceCoordinator) {
    return;
  }
  if ([self shouldShowChoiceScreen]) {
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
  if (!ios::provider::IsChoiceEnabled()) {
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

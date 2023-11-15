// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/search_engine_choice/promo/search_engine_choice_scene_agent.h"

#import <memory>

#import "base/check.h"
#import "components/search_engines/search_engine_choice_utils.h"
#import "ios/chrome/browser/policy/browser_state_policy_connector.h"
#import "ios/chrome/browser/promos_manager/promos_manager.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"

@interface SearchEngineChoiceSceneAgent ()

@property(nonatomic, assign) PromosManager* promosManager;

@end

@implementation SearchEngineChoiceSceneAgent {
  base::WeakPtr<ChromeBrowserState> _browserState;
}

- (instancetype)initWithPromosManager:(PromosManager*)promosManager
                      forBrowserState:(ChromeBrowserState*)browserState {
  self = [super init];
  if (self) {
    self.promosManager = promosManager;
    if (browserState) {
      _browserState = browserState->AsWeakPtr();
      CHECK(!_browserState->IsOffTheRecord());
    }
  }
  return self;
}

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  switch (level) {
    case SceneActivationLevelForegroundActive:
    case SceneActivationLevelBackground:
    case SceneActivationLevelForegroundInactive: {
      [self registerPromoForSingleDisplayIfNecessary];
      break;
    }
    case SceneActivationLevelDisconnected:
    case SceneActivationLevelUnattached: {
      break;
    }
  }
}

// Register the Search Engine Choice Screen for a single display in the promo
// manager if we want to display the choice screen promo.
- (void)registerPromoForSingleDisplayIfNecessary {
  ChromeBrowserState* browserState = _browserState.get();
  if (!browserState) {
    return;
  }
  CHECK(self.promosManager);
  BrowserStatePolicyConnector* policyConnector =
      browserState->GetPolicyConnector();
  if (search_engines::ShouldShowChoiceScreen(
          *policyConnector->GetPolicyService(),
          /*profile_properties=*/
          {.is_regular_profile = true,
           .pref_service = browserState->GetPrefs()},
          ios::TemplateURLServiceFactory::GetForBrowserState(browserState))) {
    self.promosManager->RegisterPromoForSingleDisplay(
        promos_manager::Promo::Choice);
  } else {
    // If the promo was not registered to begin with, this does nothing.
    self.promosManager->DeregisterPromo(promos_manager::Promo::Choice);
  }
}

@end

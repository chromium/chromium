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

@end

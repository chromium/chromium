// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/omnibox_position/promo/omnibox_position_choice_scene_agent.h"

#import <memory>

#import "base/check.h"
#import "ios/chrome/browser/policy/browser_state_policy_connector.h"
#import "ios/chrome/browser/promos_manager/promos_manager.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/ui/first_run/omnibox_position/omnibox_position_choice_util.h"

@interface OmniboxPositionChoiceSceneAgent () <BooleanObserver>

@property(nonatomic, assign) PromosManager* promosManager;

@end

@implementation OmniboxPositionChoiceSceneAgent {
  base::WeakPtr<ChromeBrowserState> _browserState;
  /// Pref tracking if the preferred omnibox position is bottom.
  PrefBackedBoolean* _bottomOmniboxPref;
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

#pragma mark - SceneStateObserver

- (void)sceneStateDidEnableUI:(SceneState*)sceneState {
  [self setupObservers];
}

- (void)sceneStateDidDisableUI:(SceneState*)sceneState {
  [self tearDownObservers];
}

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  switch (level) {
    case SceneActivationLevelForegroundActive:
    case SceneActivationLevelBackground:
    case SceneActivationLevelForegroundInactive: {
      [self registerPromoIfNecessary];
      break;
    }
    case SceneActivationLevelDisconnected:
    case SceneActivationLevelUnattached: {
      break;
    }
  }
}

#pragma mark - Boolean Observer

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  ChromeBrowserState* browserState = _browserState.get();
  if (!browserState) {
    return;
  }

  if (observableBoolean == _bottomOmniboxPref) {
    if (browserState->GetPrefs() && self.promosManager &&
        !ShouldShowOmniboxPositionChoiceIPHPromo(browserState->GetPrefs())) {
      self.promosManager->DeregisterPromo(
          promos_manager::Promo::OmniboxPosition);
    }
  }
}

#pragma mark - Private

/// Register the Omnibox Position Choice Screen in the promo manager if we want
/// to display the promo.
- (void)registerPromoIfNecessary {
  ChromeBrowserState* browserState = _browserState.get();
  if (!browserState) {
    return;
  }
  CHECK(self.promosManager);
  if (ShouldShowOmniboxPositionChoiceIPHPromo(browserState->GetPrefs())) {
    self.promosManager->RegisterPromoForContinuousDisplay(
        promos_manager::Promo::OmniboxPosition);
  } else {
    self.promosManager->DeregisterPromo(promos_manager::Promo::OmniboxPosition);
  }
}

/// Sets up pref observation.
- (void)setupObservers {
  ChromeBrowserState* browserState = _browserState.get();
  if (!browserState) {
    return;
  }
  _bottomOmniboxPref =
      [[PrefBackedBoolean alloc] initWithPrefService:browserState->GetPrefs()
                                            prefName:prefs::kBottomOmnibox];
  [_bottomOmniboxPref setObserver:self];
  // Initialize to the correct value.
  [self booleanDidChange:_bottomOmniboxPref];
}

/// Tears down pref observation.
- (void)tearDownObservers {
  [_bottomOmniboxPref stop];
  [_bottomOmniboxPref setObserver:nil];
  _bottomOmniboxPref = nil;
}

@end

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browser_state_metrics/model/browser_state_activity_app_agent.h"

#import "base/time/time.h"
#import "components/signin/core/browser/active_primary_accounts_metrics_recorder.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"

@implementation BrowserStateActivityAppAgent

- (instancetype)init {
  self = [super init];
  return self;
}

#pragma mark - Private methods

- (void)recordActivationForSceneState:(SceneState*)sceneState {
  ChromeBrowserState* browserState =
      sceneState.appState.mainProfile.browserState;

  // Update the BrowserState's last-active time in the info cache.
  BrowserStateInfoCache* infoCache = GetApplicationContext()
                                         ->GetChromeBrowserStateManager()
                                         ->GetBrowserStateInfoCache();
  int index = infoCache->GetIndexOfBrowserStateWithName(
      browserState->GetBrowserStateName());
  infoCache->SetLastActiveTimeOfBrowserStateAtIndex(index, base::Time::Now());

  // Update the primary account's last-active time (if there is a primary
  // account).
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForBrowserState(browserState);
  signin::ActivePrimaryAccountsMetricsRecorder* activeAccountsTracker =
      GetApplicationContext()->GetActivePrimaryAccountsMetricsRecorder();
  // IdentityManager is null for incognito profiles.
  if (activeAccountsTracker && identityManager &&
      identityManager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    CoreAccountInfo accountInfo =
        identityManager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
    activeAccountsTracker->MarkAccountAsActiveNow(accountInfo.gaia);
  }
}

#pragma mark - AppStateObserver

- (void)appState:(AppState*)appState
    didTransitionFromInitStage:(InitStage)previousInitStage {
  [super appState:appState didTransitionFromInitStage:previousInitStage];

  if (appState.initStage != InitStageBrowserObjectsForUI) {
    return;
  }

  // Check if any scene has already reached foreground active state.
  for (SceneState* sceneState in appState.connectedScenes) {
    if (sceneState.activationLevel == SceneActivationLevelForegroundActive) {
      [self recordActivationForSceneState:sceneState];
    }
  }
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  [super sceneState:sceneState transitionedToActivationLevel:level];

  // Ignore if the app has not loaded the ChromeBrowserState yet.
  if (sceneState.appState.initStage < InitStageBrowserObjectsForUI) {
    return;
  }

  if (level == SceneActivationLevelForegroundActive) {
    [self recordActivationForSceneState:sceneState];
  }
}

@end

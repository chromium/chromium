// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browser_state_metrics/model/browser_state_activity_scene_agent.h"

#import "base/time/time.h"
#import "components/signin/core/browser/active_primary_accounts_metrics_recorder.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state_manager.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"

@implementation BrowserStateActivitySceneAgent

- (instancetype)init {
  self = [super init];
  return self;
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  if (level == SceneActivationLevelForegroundActive) {
    ChromeBrowserState* browserState =
        self.sceneState.browserProviderInterface.mainBrowserProvider.browser
            ->GetBrowserState();

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
}

@end

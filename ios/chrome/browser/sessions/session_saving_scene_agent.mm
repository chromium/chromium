// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/session_saving_scene_agent.h"

#import "ios/chrome/browser/sessions/session_restoration_service.h"
#import "ios/chrome/browser/sessions/session_restoration_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/web_state_list/model/web_usage_enabler/web_usage_enabler_browser_agent.h"

@implementation SessionSavingSceneAgent {
  // YES when sessions need saving -- specifically after the scene has
  // foregrounded. Initially NO, so session saving isn't triggered as the
  // scene initially launches.
  BOOL _sessionsNeedSaving;
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  switch (level) {
    case SceneActivationLevelUnattached:
    case SceneActivationLevelDisconnected:
      // no-op.
      break;
    case SceneActivationLevelBackground:
      [self saveSessionsIfNeeded];
      break;
    case SceneActivationLevelForegroundInactive:
      [self prepareSessionsForBackgrounding];
      break;
    case SceneActivationLevelForegroundActive:
      _sessionsNeedSaving = YES;
      break;
  }
}

#pragma mark - Public

- (void)saveSessionsIfNeeded {
  // No need to do this if the session is already saved.
  if (!_sessionsNeedSaving)
    return;

  id<BrowserProviderInterface> browserProviderInterface =
      self.sceneState.browserProviderInterface;
  if (!browserProviderInterface) {
    return;
  }

  // Since the app is about to be backgrounded or terminated, save the sessions
  // immediately for the main BrowserState and, if it exists, the incognito
  // BrowserState.
  ChromeBrowserState* mainBrowserState =
      browserProviderInterface.mainBrowserProvider.browser->GetBrowserState();
  SessionRestorationServiceFactory::GetForBrowserState(mainBrowserState)
      ->SaveSessions();

  if (browserProviderInterface.hasIncognitoBrowserProvider) {
    ChromeBrowserState* incognitoBrowserstate =
        browserProviderInterface.incognitoBrowserProvider.browser
            ->GetBrowserState();
    SessionRestorationServiceFactory::GetForBrowserState(incognitoBrowserstate)
        ->SaveSessions();
  }

  // Save a grey version of the active webstates.
  SnapshotTabHelper* mainSnapshotHelper =
      [self snapshotHelperForActiveWebStateInMainBrowser];
  if (mainSnapshotHelper) {
    mainSnapshotHelper->SaveGreyInBackground();
  }

  SnapshotTabHelper* incognitoSnapshotHelper =
      [self snapshotHelperForActiveWebStateInIncognitoBrowser];
  if (incognitoSnapshotHelper) {
    incognitoSnapshotHelper->SaveGreyInBackground();
  }

  _sessionsNeedSaving = NO;
}

#pragma mark - Private

- (void)prepareSessionsForBackgrounding {
  SnapshotTabHelper* mainSnapshotHelper =
      [self snapshotHelperForActiveWebStateInMainBrowser];
  if (mainSnapshotHelper) {
    mainSnapshotHelper->WillBeSavedGreyWhenBackgrounding();
  }

  SnapshotTabHelper* incognitoSnapshotHelper =
      [self snapshotHelperForActiveWebStateInIncognitoBrowser];
  if (incognitoSnapshotHelper) {
    incognitoSnapshotHelper->WillBeSavedGreyWhenBackgrounding();
  }
}

#pragma mark - Utility

- (SnapshotTabHelper*)snapshotHelperForActiveWebStateInMainBrowser {
  id<BrowserProviderInterface> browserProviderInterface =
      self.sceneState.browserProviderInterface;
  if (!browserProviderInterface) {
    return nullptr;
  }

  return [self snapshotHelperForActiveWebStateInBrowser:browserProviderInterface
                                                            .mainBrowserProvider
                                                            .browser];
}

- (SnapshotTabHelper*)snapshotHelperForActiveWebStateInIncognitoBrowser {
  id<BrowserProviderInterface> browserProviderInterface =
      self.sceneState.browserProviderInterface;
  if (!browserProviderInterface.hasIncognitoBrowserProvider) {
    return nullptr;
  }

  return [self
      snapshotHelperForActiveWebStateInBrowser:browserProviderInterface
                                                   .incognitoBrowserProvider
                                                   .browser];
}

- (SnapshotTabHelper*)snapshotHelperForActiveWebStateInBrowser:
    (Browser*)browser {
  WebUsageEnablerBrowserAgent* webEnabler =
      WebUsageEnablerBrowserAgent::FromBrowser(browser);
  web::WebState* webState = browser->GetWebStateList()->GetActiveWebState();
  if (webEnabler->IsWebUsageEnabled() && webState != nullptr) {
    return SnapshotTabHelper::FromWebState(webState);
  }

  return nullptr;
}

@end

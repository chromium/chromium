// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/start_surface/start_surface_scene_agent.h"
#import "base/feature_list.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/startup_information.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/tab_insertion/model/tab_insertion_browser_agent.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_features.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_recent_tab_browser_agent.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_util.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "url/gurl.h"

namespace {
// Name of histogram to record the number of excess NTP tabs that are removed.
const char kExcessNTPTabsRemoved[] = "IOS.NTP.ExcessRemovedTabCount";
}  // namespace

@interface StartSurfaceSceneAgent () <AppStateObserver>

// Caches the previous activation level.
@property(nonatomic, assign) SceneActivationLevel previousActivationLevel;

// YES if The AppState was not ready before the SceneState reached a valid
// activation level, so therefore this agent needs to wait for the AppState's
// initStage to reach a valid stage before checking whether the Start Surface
// should be shown.
@property(nonatomic, assign) BOOL waitingForAppStateAfterSceneStateReady;

@end

@implementation StartSurfaceSceneAgent

- (id)init {
  self = [super init];
  if (self) {
    self.previousActivationLevel = SceneActivationLevelUnattached;
  }
  return self;
}

#pragma mark - ObservingSceneAgent

- (void)setSceneState:(SceneState*)sceneState {
  [super setSceneState:sceneState];

  [self.sceneState.appState addObserver:self];
}

#pragma mark - AppStateObserver

- (void)appState:(AppState*)appState
    didTransitionFromInitStage:(InitStage)previousInitStage {
  if (appState.initStage >= InitStageFirstRun &&
      self.waitingForAppStateAfterSceneStateReady) {
    self.waitingForAppStateAfterSceneStateReady = NO;
    [self showStartSurfaceIfNecessary];
  }
}

#pragma mark - SceneStateObserver

- (void)sceneStateDidDisableUI:(SceneState*)sceneState {
  // Tear down objects tied to the scene state before it is deleted.
  [self.sceneState.appState removeObserver:self];
  self.waitingForAppStateAfterSceneStateReady = NO;
}

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  if (level != SceneActivationLevelForegroundActive &&
      self.previousActivationLevel == SceneActivationLevelForegroundActive) {
    // TODO(crbug.com/1173160): Consider when to clear the session object since
    // Chrome may be closed without transiting to inactive, e.g. device power
    // off, then the previous session object is staled.
    SetStartSurfaceSessionObjectForSceneState(sceneState);
  }
  if (level == SceneActivationLevelBackground &&
      self.previousActivationLevel > SceneActivationLevelBackground) {
    if (base::FeatureList::IsEnabled(kRemoveExcessNTPs)) {
      // Remove duplicate NTP pages upon background event.
      if (self.sceneState.browserProviderInterface.mainBrowserProvider
              .browser) {
        [self removeExcessNTPsInBrowser:self.sceneState.browserProviderInterface
                                            .mainBrowserProvider.browser];
      }
      if (self.sceneState.browserProviderInterface.incognitoBrowserProvider
              .browser) {
        [self removeExcessNTPsInBrowser:self.sceneState.browserProviderInterface
                                            .incognitoBrowserProvider.browser];
      }
    }
  }
  if (level >= SceneActivationLevelForegroundInactive &&
      self.previousActivationLevel < SceneActivationLevelForegroundInactive) {
    [self logBackgroundDurationMetricForActivationLevel:level];
    [self showStartSurfaceIfNecessary];
  }
  self.previousActivationLevel = level;
}

- (void)showStartSurfaceIfNecessary {
  if (self.sceneState.appState.initStage <= InitStageFirstRun) {
    // NO if the app is not yet ready to present normal UI that is required by
    // Start Surface.
    self.waitingForAppStateAfterSceneStateReady = YES;
    return;
  }

  if (!ShouldShowStartSurfaceForSceneState(self.sceneState)) {
    return;
  }

  // Do not show the Start Surface no matter whether it is enabled or not when
  // the Tab grid is active by design.
  if (self.sceneState.controller.isTabGridVisible) {
    return;
  }

  // If there is no active tab, a NTP will be added, and since there is no
  // recent tab.
  // Keep showing the last active NTP tab no matter whether the Start Surface is
  // enabled or not by design.
  // Note that activeWebState could only be nullptr when the Tab grid is active
  // for now.
  web::WebState* activeWebState =
      self.sceneState.browserProviderInterface.mainBrowserProvider.browser
          ->GetWebStateList()
          ->GetActiveWebState();
  if (!activeWebState || IsUrlNtp(activeWebState->GetVisibleURL())) {
    return;
  }

  base::RecordAction(base::UserMetricsAction("IOS.StartSurface.Show"));
  Browser* browser =
      self.sceneState.browserProviderInterface.mainBrowserProvider.browser;
  StartSurfaceRecentTabBrowserAgent::FromBrowser(browser)->SaveMostRecentTab();

  // Activate the existing NTP tab for the Start surface.
  WebStateList* webStateList = browser->GetWebStateList();
  for (int i = 0; i < webStateList->count(); i++) {
    web::WebState* webState = webStateList->GetWebStateAt(i);
    if (IsUrlNtp(webState->GetVisibleURL())) {
      NewTabPageTabHelper::FromWebState(webState)->SetShowStartSurface(true);
      webStateList->ActivateWebStateAt(i);
      return;
    }
  }

  // Create a new NTP since there is no existing one.
  TabInsertionBrowserAgent* insertion_agent =
      TabInsertionBrowserAgent::FromBrowser(browser);
  web::NavigationManager::WebLoadParams web_load_params(
      (GURL(kChromeUINewTabURL)));
  TabInsertion::Params tab_insertion_params;
  tab_insertion_params.should_show_start_surface = true;
  insertion_agent->InsertWebState(web_load_params, tab_insertion_params);
}

// Removes duplicate NTP tabs in `browser`'s WebStateList.
- (void)removeExcessNTPsInBrowser:(Browser*)browser {
  WebStateList* webStateList = browser->GetWebStateList();
  web::WebState* activeWebState =
      browser->GetWebStateList()->GetActiveWebState();
  int activeWebStateIndex = webStateList->GetIndexOfWebState(activeWebState);
  NSMutableArray<NSNumber*>* emptyNtpIndices = [[NSMutableArray alloc] init];
  web::WebState* lastNtpWebStatesWithNavHistory = nullptr;
  BOOL keepOneNTP = YES;
  BOOL activeWebStateIsEmptyNTP = NO;
  for (int i = 0; i < webStateList->count(); i++) {
    web::WebState* webState = webStateList->GetWebStateAt(i);
    if (IsUrlNtp(webState->GetVisibleURL())) {
      // Check if there is navigation history for this WebState that is showing
      // the NTP. If there is, then set `keepOneNTP` to NO, indicating that all
      // WebStates in NTPs with no navigation history will get removed.
      if (webState->GetNavigationItemCount() == 1) {
        // Keep track if active WebState is showing an NTP and has no navigation
        // history since it may get removed if `keepOneNTP` is NO.
        if (i == activeWebStateIndex) {
          activeWebStateIsEmptyNTP = YES;
        }
        // Insert at the front so that iterating through the array will remove
        // WebStates in descending index order, preventing WebState indices from
        // changing during removal.
        [emptyNtpIndices insertObject:@(i) atIndex:0];
      } else {
        keepOneNTP = NO;
        lastNtpWebStatesWithNavHistory = webState;
      }
    }
  }
  if (keepOneNTP) {
    // If the current active tab may be removed because it is showing the NTP
    // and has no navigation history, then save that tab. Otherwise, keep the
    // first index to save the most recently created tab.
    NSNumber* tabIndexToSave = [emptyNtpIndices firstObject];
    if (activeWebStateIsEmptyNTP &&
        [[emptyNtpIndices lastObject] intValue] != activeWebStateIndex) {
      tabIndexToSave = @(activeWebStateIndex);
    }
    [emptyNtpIndices removeObject:tabIndexToSave];
  }
  UMA_HISTOGRAM_COUNTS_100(kExcessNTPTabsRemoved, [emptyNtpIndices count]);
  // Removal starts from higher indices to ensure tab indices stay fixed
  // throughout removal process.
  for (NSNumber* index in emptyNtpIndices) {
    web::WebState* webState =
        browser->GetWebStateList()->GetWebStateAt([index intValue]);
    DCHECK(IsUrlNtp(webState->GetVisibleURL()));
    webStateList->CloseWebStateAt([index intValue],
                                  WebStateList::CLOSE_NO_FLAGS);
  }
  // If the active WebState was removed because it was showing the NTP and had
  // no navigation history, switch to another NTP. This only is needed if there
  // were tabs showing the NTP that had navigation histories. Otherwise, code
  // above already saves the current active WebState right before empty NTPs are
  // removed.
  if (activeWebStateIsEmptyNTP && !keepOneNTP) {
    DCHECK(lastNtpWebStatesWithNavHistory);
    int newActiveIndex =
        webStateList->GetIndexOfWebState(lastNtpWebStatesWithNavHistory);
    webStateList->ActivateWebStateAt(newActiveIndex);
  }
}

- (void)logBackgroundDurationMetricForActivationLevel:
    (SceneActivationLevel)level {
  const base::TimeDelta timeSinceBackground =
      GetTimeSinceMostRecentTabWasOpenForSceneState(self.sceneState);
  const BOOL isColdStart =
      (level > SceneActivationLevelBackground &&
       self.sceneState.appState.startupInformation.isColdStart);
  if (isColdStart) {
    UMA_HISTOGRAM_CUSTOM_COUNTS("IOS.BackgroundTimeBeforeColdStart",
                                timeSinceBackground.InMinutes(), 1,
                                base::Hours(12).InMinutes(), 24);
  } else {
    UMA_HISTOGRAM_CUSTOM_COUNTS("IOS.BackgroundTimeBeforeWarmStart",
                                timeSinceBackground.InMinutes(), 1,
                                base::Hours(12).InMinutes(), 24);
  }
}

@end

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/start_surface/start_surface_scene_agent.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/startup_information.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/chrome_url_util.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/ui/main/browser_interface_provider.h"
#import "ios/chrome/browser/ui/main/scene_controller.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_features.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_recent_tab_browser_agent.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_util.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/web_state_list/tab_insertion_browser_agent.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Name of histogram to record the number of excess NTP tabs that are removed.
const char kExcessNTPTabsRemoved[] = "IOS.NTP.ExcessRemovedTabCount";
}  // namespace

@interface StartSurfaceSceneAgent ()

// Caches the previous activation level.
@property(nonatomic, assign) SceneActivationLevel previousActivationLevel;

@end

@implementation StartSurfaceSceneAgent

- (id)init {
  self = [super init];
  if (self) {
    self.previousActivationLevel = SceneActivationLevelUnattached;
  }
  return self;
}

#pragma mark - SceneStateObserver

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
      [self removeExcessNTPsInBrowser:self.sceneState.interfaceProvider
                                          .mainInterface.browser];
      [self removeExcessNTPsInBrowser:self.sceneState.interfaceProvider
                                          .incognitoInterface.browser];
    }
  }
  if (level >= SceneActivationLevelForegroundInactive &&
      self.previousActivationLevel < SceneActivationLevelForegroundInactive) {
    if (IsStartSurfaceSplashStartupEnabled()) {
      [self logBackgroundDurationMetricForActivationLevel:level];
      [self showStartSurfaceIfNecessary];
    }
  }
  self.previousActivationLevel = level;
}

- (void)showStartSurfaceIfNecessary {
  if (!ShouldShowStartSurfaceForSceneState(self.sceneState)) {
    return;
  }

  // Do not show the Start Surface no matter whether it is enabled or not when
  // the Tab grid is active by design.
  if (self.sceneState.controller.isTabGridVisible) {
    return;
  }

  // If there is no active tab, a NTP will be added, and since there is no
  // recent tab, there is no need to mark |modifytVisibleNTPForStartSurface|.
  // Keep showing the last active NTP tab no matter whether the Start Surface is
  // enabled or not by design.
  // Note that activeWebState could only be nullptr when the Tab grid is active
  // for now.
  web::WebState* activeWebState =
      self.sceneState.interfaceProvider.mainInterface.browser->GetWebStateList()
          ->GetActiveWebState();
  if (!activeWebState || IsURLNtp(activeWebState->GetVisibleURL())) {
    return;
  }

  base::RecordAction(base::UserMetricsAction("IOS.StartSurface.Show"));
  Browser* browser = self.sceneState.interfaceProvider.mainInterface.browser;
  StartSurfaceRecentTabBrowserAgent::FromBrowser(browser)->SaveMostRecentTab();

  // Activate the existing NTP tab for the Start surface.
  WebStateList* webStateList = browser->GetWebStateList();
  for (int i = 0; i < webStateList->count(); i++) {
    web::WebState* webState = webStateList->GetWebStateAt(i);
    if (IsURLNtp(webState->GetVisibleURL())) {
      NewTabPageTabHelper::FromWebState(webState)->SetShowStartSurface(true);
      webStateList->ActivateWebStateAt(i);
      return;
    }
  }

  // Create a new NTP since there is no existing one.
  TabInsertionBrowserAgent* insertion_agent =
      TabInsertionBrowserAgent::FromBrowser(browser);
  web::NavigationManager::WebLoadParams params((GURL(kChromeUINewTabURL)));
  insertion_agent->InsertWebState(
      params, nullptr, /*opened_by_dom=*/false,
      TabInsertion::kPositionAutomatically, /*in_background=*/false,
      /*inherit_opener=*/false, /*should_show_start_surface=*/true);
}

// Removes duplicate NTP tabs in |browser|'s WebStateList.
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
    if (IsURLNtp(webState->GetVisibleURL())) {
      // Check if there is navigation history for this WebState that is showing
      // the NTP. If there is, then set |keepOneNTP| to NO, indicating that all
      // WebStates in NTPs with no navigation history will get removed.
      if (webState->GetNavigationItemCount() == 1) {
        // Keep track if active WebState is showing an NTP and has no navigation
        // history since it may get removed if |keepOneNTP| is NO.
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
    DCHECK(IsURLNtp(webState->GetVisibleURL()));
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
  NSInteger timeSinceBackgroundInMinutes =
      GetTimeSinceMostRecentTabWasOpenForSceneState(self.sceneState) / 60;
  BOOL isColdStart = (level > SceneActivationLevelBackground &&
                      self.sceneState.appState.startupInformation.isColdStart);
  if (isColdStart) {
    base::UmaHistogramCustomTimes("IOS.ColdStartBackgroundTime",
                                  base::Minutes(timeSinceBackgroundInMinutes),
                                  base::Seconds(0),
                                  base::Seconds(12 * 60 /* 12 hours */), 24);
  } else {
    base::UmaHistogramCustomTimes("IOS.WarmStartBackgroundTime",
                                  base::Minutes(timeSinceBackgroundInMinutes),
                                  base::Seconds(0),
                                  base::Seconds(12 * 60 /* 12 hours */), 24);
  }
}

@end

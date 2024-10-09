// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_scene_agent.h"

#import "base/containers/contains.h"
#import "base/feature_list.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/startup_information.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/profile/profile_state_observer.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/removing_indexes.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/signin_util.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_features.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_recent_tab_browser_agent.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_util.h"
#import "ios/chrome/browser/tab_insertion/model/tab_insertion_browser_agent.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "url/gurl.h"

namespace {

// Name of histogram to record the number of excess NTP tabs that are removed.
const char kExcessNTPTabsRemoved[] = "IOS.NTP.ExcessRemovedTabCount";

// Whether `web_state` shows the NTP.
bool IsNTP(const web::WebState* web_state) {
  return IsUrlNtp(web_state->GetVisibleURL());
}

// Whether `web_state` shows the NTP and never had a navigation.
bool IsEmptyNTP(const web::WebState* web_state) {
  return IsNTP(web_state) && web_state->GetNavigationItemCount() <= 1;
}

}  // namespace

@interface StartSurfaceSceneAgent () <ProfileStateObserver>

// Caches the previous activation level.
@property(nonatomic, assign) SceneActivationLevel previousActivationLevel;

// YES if The ProfileState was not ready before the SceneState reached a valid
// activation level, so therefore this agent needs to wait for the ProfileState
// initStage to reach a valid stage before checking whether the Start Surface
// should be shown.
@property(nonatomic, assign) BOOL waitingForProfileStateAfterSceneStateReady;

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

  [self.sceneState.profileState addObserver:self];
}

#pragma mark - ProfileStateObserver

- (void)profileState:(ProfileState*)profileState
    didTransitionToInitStage:(ProfileInitStage)nextInitStage
               fromInitStage:(ProfileInitStage)fromInitStage {
  if (nextInitStage == ProfileInitStage::kFinal &&
      self.waitingForProfileStateAfterSceneStateReady) {
    self.waitingForProfileStateAfterSceneStateReady = NO;
    [self showStartSurfaceIfNecessary];
  }
}

#pragma mark - SceneStateObserver

- (void)sceneStateDidDisableUI:(SceneState*)sceneState {
  // Tear down objects tied to the scene state before it is deleted.
  [self.sceneState.profileState removeObserver:self];
  self.waitingForProfileStateAfterSceneStateReady = NO;
}

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  if (level != SceneActivationLevelForegroundActive &&
      self.previousActivationLevel == SceneActivationLevelForegroundActive) {
    // TODO(crbug.com/40167003): Consider when to clear the session object since
    // Chrome may be closed without transiting to inactive, e.g. device power
    // off, then the previous session object is staled.
    SetStartSurfaceSessionObjectForSceneState(sceneState);
  }
  if (level == SceneActivationLevelBackground &&
      self.previousActivationLevel > SceneActivationLevelBackground) {
    if (base::FeatureList::IsEnabled(kRemoveExcessNTPs)) {
      // Remove duplicate NTP pages upon background event.
      [self removeExcessNTPs];
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
  if (self.sceneState.profileState.initStage < ProfileInitStage::kFinal) {
    // NO if the app is not yet ready to present normal UI that is required by
    // Start Surface.
    self.waitingForProfileStateAfterSceneStateReady = YES;
    return;
  }

  Browser* browser =
      self.sceneState.browserProviderInterface.mainBrowserProvider.browser;
  // TODO(crbug.com/343699504): Remove pre-fetching capabilities once these
  // are loaded in iSL.
  RunSystemCapabilitiesPrefetch(
      ChromeAccountManagerServiceFactory::GetForProfile(browser->GetProfile())
          ->GetAllIdentities());

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
      browser->GetWebStateList()->GetActiveWebState();
  if (!activeWebState || IsUrlNtp(activeWebState->GetVisibleURL())) {
    return;
  }

  base::RecordAction(base::UserMetricsAction("IOS.StartSurface.Show"));
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

// Removes empty NTP tabs.
- (void)removeExcessNTPs {
  id<BrowserProviderInterface> providerInterface =
      self.sceneState.browserProviderInterface;

  [self removeExcessNTPsInBrowser:providerInterface.mainBrowserProvider];
  [self removeExcessNTPsInBrowser:providerInterface.incognitoBrowserProvider];
}

// Removes empty NTP tabs (i.e. NTPs with no further navigation) in the
// WebStateList attached to the Browser accessible via `browserProvider`
//
// NTPs with navigation are all preserved. If there are none, an empty NTP is
// preserved.
- (void)removeExcessNTPsInBrowser:(id<BrowserProvider>)browserProvider {
  Browser* browser = browserProvider.browser;
  if (!browser) {
    return;
  }

  WebStateList* webStateList = browser->GetWebStateList();
  DCHECK(webStateList);

  // Map groups to the indices of its empty NTPs, and whether the group contains
  // at least one non-empty NTP (an NTP with navigation), which will be kept.
  // Ungrouped tabs correspond to the `nullptr` entry in the map.
  std::map<const TabGroup*, std::pair<std::vector<int>, bool>> groupsToNTPs;
  for (int index = 0; index < webStateList->count(); ++index) {
    const web::WebState* webState = webStateList->GetWebStateAt(index);
    const TabGroup* tabGroup = webStateList->GetGroupOfWebStateAt(index);
    if (IsEmptyNTP(webState)) {
      groupsToNTPs[tabGroup].first.push_back(index);
    } else if (IsNTP(webState)) {
      groupsToNTPs[tabGroup].second = true;
    }
  }

  // For each group (respectively the ungrouped tabs case), if there are only
  // empty NTPs, preserve one NTP by removing it from the list of indices to
  // close for the group (respectively the ungrouped tabs case).
  for (auto& [group, NTPs] : groupsToNTPs) {
    auto& indicesToRemoveInGroup = NTPs.first;
    const bool groupHasNonEmptyNTP = NTPs.second;
    if (indicesToRemoveInGroup.empty() || groupHasNonEmptyNTP) {
      continue;
    }
    // Remove the last empty NTP from the list of tabs to close.
    indicesToRemoveInGroup.pop_back();
  }

  // Flatten the list of indices to remove.
  std::vector<int> indicesToRemove;
  for (const auto& [group, NTPs] : groupsToNTPs) {
    const auto& indicesToRemoveInGroup = NTPs.first;
    indicesToRemove.insert(indicesToRemove.end(),
                           indicesToRemoveInGroup.begin(),
                           indicesToRemoveInGroup.end());
  }

  // Report how many, if any, excess NTPs have been removed.
  UMA_HISTOGRAM_COUNTS_100(kExcessNTPTabsRemoved, indicesToRemove.size());

  // Perform the operations on the WebStateList, if needed.
  if (indicesToRemove.empty()) {
    return;
  }
  const WebStateList::ScopedBatchOperation batch =
      webStateList->StartBatchOperation();

  // If the active tab is going to be closed, pick the last ungrouped
  // NTP as the new active tab, otherwise insert a new NTP.
  if (base::Contains(indicesToRemove, webStateList->active_index())) {
    int lastUngroupedNTPIndex = WebStateList::kInvalidIndex;
    for (int index = webStateList->count() - 1; index >= 0; --index) {
      const web::WebState* webState = webStateList->GetWebStateAt(index);
      const TabGroup* tabGroup = webStateList->GetGroupOfWebStateAt(index);
      if (IsNTP(webState) && !tabGroup &&
          !base::Contains(indicesToRemove, index)) {
        lastUngroupedNTPIndex = index;
        break;
      }
    }
    if (lastUngroupedNTPIndex != WebStateList::kInvalidIndex) {
      webStateList->ActivateWebStateAt(lastUngroupedNTPIndex);
    } else {
      // Insert a new NTP at the very end (this won't invalidate other indices).
      web::NavigationManager::WebLoadParams webLoadParams =
          web::NavigationManager::WebLoadParams(GURL(kChromeUINewTabURL));
      TabInsertion::Params tabInsertionParams;
      tabInsertionParams.should_skip_new_tab_animation = true;
      TabInsertionBrowserAgent::FromBrowser(browser)->InsertWebState(
          webLoadParams, tabInsertionParams);
    }
  }

  // Close the excessive NTPs.
  webStateList->CloseWebStatesAtIndices(
      WebStateList::CLOSE_NO_FLAGS,
      RemovingIndexes(std::move(indicesToRemove)));
}

- (void)logBackgroundDurationMetricForActivationLevel:
    (SceneActivationLevel)level {
  const base::TimeDelta timeSinceBackground =
      GetTimeSinceMostRecentTabWasOpenForSceneState(self.sceneState);
  const BOOL isColdStart =
      (level > SceneActivationLevelBackground &&
       self.sceneState.profileState.appState.startupInformation.isColdStart);
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

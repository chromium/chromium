// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/metrics/tab_usage_recorder.h"

#import <UIKit/UIKit.h>

#include "base/metrics/histogram_macros.h"
#include "components/ukm/ios/ukm_url_recorder.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/metrics/previous_session_info.h"
#import "ios/chrome/browser/prerender/prerender_service.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "ui/base/page_transition_types.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// The histogram recording the state of the tab the user switches to.
const char kSelectedTabHistogramName[] =
    "Tab.StatusWhenSwitchedBackToForeground";

// The histogram to record the number of page loads before an evicted tab is
// selected.
const char kPageLoadsBeforeEvictedTabSelected[] =
    "Tab.PageLoadsSinceLastSwitchToEvictedTab";

// Records the time it takes for an evicted tab to reload.
const char kEvictedTabReloadTime[] = "Tab.RestoreTime";

// Records success vs failure of an evicted tab's reload.
const char kEvictedTabReloadSuccessRate[] = "Tab.RestoreResult";

// Records whether or not the user switched tabs before an evicted tab finished
// reloading.
const char kDidUserWaitForEvictedTabReload[] = "Tab.RestoreUserPersistence";

// The name of the histogram that records time intervals between tab restores.
const char kTimeBetweenRestores[] = "Tabs.TimeBetweenRestores";

// The name of the histogram that records time intervals since the last restore.
const char kTimeAfterLastRestore[] = "Tabs.TimeAfterLastRestore";

// Name of histogram to record whether a memory warning had been recently
// received when a renderer termination occurred.
const char kRendererTerminationSawMemoryWarning[] =
    "Tab.RendererTermination.RecentlyReceivedMemoryWarning";

// Name of histogram to record the number of alive renderers when a renderer
// termination is received.
const char kRendererTerminationAliveRenderers[] =
    "Tab.RendererTermination.AliveRenderersCount";

// Name of histogram to record the number of renderers that were alive shortly
// before a renderer termination. This metric is being recorded in case the OS
// kills renderers in batches.
const char kRendererTerminationRecentlyAliveRenderers[] =
    "Tab.RendererTermination.RecentlyAliveRenderersCount";

// Name of histogram for recording the state of the tab when the renderer is
// terminated.
const char kRendererTerminationStateHistogram[] =
    "Tab.StateAtRendererTermination";

// The recently alive renderer count metric counts all renderers that were alive
// x seconds before a renderer termination. |kSecondsBeforeRendererTermination|
// specifies x.
const int kSecondsBeforeRendererTermination = 2;

TabUsageRecorder::TabUsageRecorder(WebStateList* web_state_list,
                                   PrerenderService* prerender_service)
    : restore_start_time_(base::TimeTicks::Now()),
      web_state_list_(web_state_list),
      prerender_service_(prerender_service) {
  DCHECK(web_state_list_);

  web_state_list_->AddObserver(this);
  for (int index = 0; index < web_state_list_->count(); ++index) {
    web::WebState* web_state = web_state_list_->GetWebStateAt(index);
    web_state->AddObserver(this);
  }

  // Register for backgrounding and foregrounding notifications. It is safe for
  // the block to capture a pointer to |this| as they are unregistered in the
  // destructor and thus the block are not called after the end of its lifetime.
  application_backgrounding_observer_ = [[NSNotificationCenter defaultCenter]
      addObserverForName:UIApplicationDidEnterBackgroundNotification
                  object:nil
                   queue:nil
              usingBlock:^(NSNotification*) {
                this->AppDidEnterBackground();
              }];

  application_foregrounding_observer_ = [[NSNotificationCenter defaultCenter]
      addObserverForName:UIApplicationWillEnterForegroundNotification
                  object:nil
                   queue:nil
              usingBlock:^(NSNotification*) {
                this->AppWillEnterForeground();
              }];
}

TabUsageRecorder::~TabUsageRecorder() {
  DCHECK(web_state_list_);

  for (int index = 0; index < web_state_list_->count(); ++index) {
    web::WebState* web_state = web_state_list_->GetWebStateAt(index);
    web_state->RemoveObserver(this);
  }
  web_state_list_->RemoveObserver(this);

  if (application_backgrounding_observer_) {
    [[NSNotificationCenter defaultCenter]
        removeObserver:application_backgrounding_observer_];
    application_backgrounding_observer_ = nil;
  }

  if (application_foregrounding_observer_) {
    [[NSNotificationCenter defaultCenter]
        removeObserver:application_foregrounding_observer_];
    application_foregrounding_observer_ = nil;
  }
}

void TabUsageRecorder::InitialRestoredTabs(
    web::WebState* active_web_state,
    const std::vector<web::WebState*>& web_states) {
#if !defined(NDEBUG)
  // Debugging check to ensure this is called at most once per run.
  // Specifically, this function is called in either of two cases:
  // 1. For a normal (not post-crash launch), during the tab model's creation.
  // It assumes that the tab model will not be deleted and recreated during the
  // application's lifecycle even if the app is backgrounded/foregrounded.
  // 2. For a post-crash launch, when the session is restored.  In that case,
  // the tab model will not have been created with existing tabs, so this
  // function will not have been called during its creation.
  static bool kColdStartTabsRecorded = false;
  static dispatch_once_t once = 0;
  dispatch_once(&once, ^{
    DCHECK(kColdStartTabsRecorded == false);
    kColdStartTabsRecorded = true;
  });
#endif

  // Do not set eviction reason on active tab since it will be reloaded without
  // being processed as a switch to the foreground tab.
  for (web::WebState* web_state : web_states) {
    if (web_state != active_web_state) {
      evicted_web_states_[web_state] = EVICTED_DUE_TO_COLD_START;
    }
  }
}

void TabUsageRecorder::RecordTabSwitched(web::WebState* old_web_state,
                                         web::WebState* new_web_state) {
  // If a tab was created to be selected, and is selected shortly thereafter,
  // it should not add its state to the "kSelectedTabHistogramName" metric.
  // |web_state_created_selected_| is reset at the first tab switch seen after
  // it was created, regardless of whether or not it was the tab selected.
  const bool was_just_created = new_web_state == web_state_created_selected_;
  web_state_created_selected_ = nullptr;

  // Disregard reselecting the same tab, but only if the mode has not changed
  // since the last time this tab was selected.  I.e. going to incognito and
  // back to normal mode is an event we want to track, but simply going into
  // stack view and back out, without changing modes, isn't.
  if (new_web_state == old_web_state && new_web_state != mode_switch_web_state_)
    return;
  mode_switch_web_state_ = nullptr;

  // Disregard opening a new tab with no previous tab. Or closing the last tab.
  if (!old_web_state || !new_web_state)
    return;

  // Before knowledge of the previous tab, |old_web_state|, is lost, see if it
  // is a previously-evicted tab still reloading.  If it is, record that the
  // user did not wait for the evicted tab to finish reloading.
  if (old_web_state == evicted_web_state_ && old_web_state != new_web_state &&
      evicted_web_state_reload_start_time_ != base::TimeTicks()) {
    UMA_HISTOGRAM_ENUMERATION(kDidUserWaitForEvictedTabReload,
                              USER_DID_NOT_WAIT, USER_BEHAVIOR_COUNT);
  }
  ResetEvictedTab();

  if (ShouldIgnoreWebState(new_web_state) || was_just_created)
    return;

  // Should never happen.  Keeping the check to ensure that the prerender logic
  // is never overlooked, should behavior at the tab_model level change.
  DCHECK(!prerender_service_ ||
         !prerender_service_->IsWebStatePrerendered(new_web_state));

  TabStateWhenSelected web_state_state = ExtractWebStateState(new_web_state);
  if (web_state_state != IN_MEMORY) {
    // Keep track of the current 'evicted' tab.
    evicted_web_state_ = new_web_state;
    evicted_web_state_state_ = web_state_state;
    UMA_HISTOGRAM_COUNTS_1M(kPageLoadsBeforeEvictedTabSelected, page_loads_);
    ResetPageLoads();
  }

  UMA_HISTOGRAM_ENUMERATION(kSelectedTabHistogramName, web_state_state,
                            TAB_STATE_COUNT);
}

void TabUsageRecorder::RecordPrimaryTabModelChange(
    bool primary_tab_model,
    web::WebState* active_web_state) {
  if (primary_tab_model) {
    // User just came back to this tab model, so record a tab selection even
    // though the current tab was reselected.
    if (mode_switch_web_state_ == active_web_state)
      RecordTabSwitched(active_web_state, active_web_state);
  } else {
    // Keep track of the selected tab when this tab model is moved to
    // background. This way when the tab model is moved to the foreground, and
    // the current tab reselected, it is handled as a tab selection rather than
    // a no-op.
    mode_switch_web_state_ = active_web_state;
  }
}

void TabUsageRecorder::RecordPageLoadStart(web::WebState* web_state) {
  if (!ShouldIgnoreWebState(web_state)) {
    page_loads_++;
    if (web_state->IsEvicted()) {
      // On the iPad, there is no notification that a tab is being re-selected
      // after changing modes.  This catches the case where the pre-incognito
      // selected tab is selected again when leaving incognito mode.
      if (mode_switch_web_state_ == web_state)
        RecordTabSwitched(web_state, web_state);
      if (evicted_web_state_ == web_state)
        RecordRestoreStartTime();
    }
  } else {
    // If there is a currently-evicted tab reloading, make sure it is recorded
    // that the user did not wait for it to load.
    if (evicted_web_state_ &&
        evicted_web_state_reload_start_time_ != base::TimeTicks()) {
      UMA_HISTOGRAM_ENUMERATION(kDidUserWaitForEvictedTabReload,
                                USER_DID_NOT_WAIT, USER_BEHAVIOR_COUNT);
    }
    ResetEvictedTab();
  }
}

void TabUsageRecorder::RecordPageLoadDone(web::WebState* web_state,
                                          bool success) {
  if (!web_state)
    return;
  if (web_state == evicted_web_state_) {
    if (success) {
      LOCAL_HISTOGRAM_TIMES(
          kEvictedTabReloadTime,
          base::TimeTicks::Now() - evicted_web_state_reload_start_time_);
    }
    UMA_HISTOGRAM_ENUMERATION(kEvictedTabReloadSuccessRate,
                              success ? LOAD_SUCCESS : LOAD_FAILURE,
                              LOAD_DONE_STATE_COUNT);

    UMA_HISTOGRAM_ENUMERATION(kDidUserWaitForEvictedTabReload, USER_WAITED,
                              USER_BEHAVIOR_COUNT);
    ResetEvictedTab();
  }
}

void TabUsageRecorder::RecordReload(web::WebState* web_state) {
  if (!ShouldIgnoreWebState(web_state)) {
    page_loads_++;
  }
}

void TabUsageRecorder::RendererTerminated(web::WebState* terminated_web_state,
                                          bool web_state_visible,
                                          bool application_active) {
  // Log the tab state for the termination.
  const RendererTerminationTabState web_state_state =
      application_active ? (web_state_visible ? FOREGROUND_TAB_FOREGROUND_APP
                                              : BACKGROUND_TAB_FOREGROUND_APP)
                         : (web_state_visible ? FOREGROUND_TAB_BACKGROUND_APP
                                              : BACKGROUND_TAB_BACKGROUND_APP);

  UMA_HISTOGRAM_ENUMERATION(kRendererTerminationStateHistogram,
                            static_cast<int>(web_state_state),
                            static_cast<int>(TERMINATION_TAB_STATE_COUNT));
  if (!web_state_visible) {
    if (WebStateAlreadyEvicted(terminated_web_state)) {
      // A web state may get notified multiple times that it's been evicted.
      // To avoid double-counting, don't do any further processing if this
      // happens.
      return;
    }
    evicted_web_states_[terminated_web_state] =
        EVICTED_DUE_TO_RENDERER_TERMINATION;
  }
  base::TimeTicks now = base::TimeTicks::Now();
  termination_timestamps_.push_back(now);

  // Log if a memory warning was seen recently.
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  BOOL saw_memory_warning =
      [defaults boolForKey:previous_session_info_constants::
                               kDidSeeMemoryWarningShortlyBeforeTerminating];
  UMA_HISTOGRAM_BOOLEAN(kRendererTerminationSawMemoryWarning,
                        saw_memory_warning);

  // Log number of live tabs after the renderer termination. This count does not
  // include |terminated_web_state|.
  int live_web_states_count = GetLiveWebStatesCount();
  UMA_HISTOGRAM_COUNTS_100(kRendererTerminationAliveRenderers,
                           live_web_states_count);

  // Clear |termination_timestamps_| of timestamps older than
  // |kSecondsBeforeRendererTermination| ago.
  base::TimeDelta seconds_before =
      base::TimeDelta::FromSeconds(kSecondsBeforeRendererTermination);
  base::TimeTicks timestamp_boundary = now - seconds_before;
  while (termination_timestamps_.front() < timestamp_boundary) {
    termination_timestamps_.pop_front();
  }

  // Log number of recently alive tabs, where recently alive is defined to mean
  // alive within the past |kSecondsBeforeRendererTermination|.
  NSUInteger recently_live_web_states_count =
      live_web_states_count + termination_timestamps_.size();
  UMA_HISTOGRAM_COUNTS_100(kRendererTerminationRecentlyAliveRenderers,
                           recently_live_web_states_count);

  ukm::SourceId source_id =
      ukm::GetSourceIdForWebStateDocument(terminated_web_state);
  if (source_id != ukm::kInvalidSourceId) {
    ukm::builders::IOS_RendererGone(source_id)
        .SetInForeground(web_state_state)
        .SetSawMemoryWarning(saw_memory_warning)
        .SetAliveRendererCount(live_web_states_count)
        .SetAliveRecentlyRendererCount(recently_live_web_states_count)
        .Record(ukm::UkmRecorder::Get());
  }
}

void TabUsageRecorder::AppDidEnterBackground() {
  base::TimeTicks time_now = base::TimeTicks::Now();
  LOCAL_HISTOGRAM_TIMES(kTimeAfterLastRestore, time_now - restore_start_time_);

  if (evicted_web_state_ &&
      evicted_web_state_reload_start_time_ != base::TimeTicks()) {
    UMA_HISTOGRAM_ENUMERATION(kDidUserWaitForEvictedTabReload, USER_LEFT_CHROME,
                              USER_BEHAVIOR_COUNT);
    ResetEvictedTab();
  }
}

void TabUsageRecorder::AppWillEnterForeground() {
  restore_start_time_ = base::TimeTicks::Now();
}

void TabUsageRecorder::ResetPageLoads() {
  page_loads_ = 0;
}

int TabUsageRecorder::EvictedTabsMapSize() {
  return evicted_web_states_.size();
}

void TabUsageRecorder::ResetAll() {
  ResetEvictedTab();
  ResetPageLoads();
  evicted_web_states_.clear();
}

void TabUsageRecorder::ResetEvictedTab() {
  evicted_web_state_ = nullptr;
  evicted_web_state_state_ = IN_MEMORY;
  evicted_web_state_reload_start_time_ = base::TimeTicks();
}

bool TabUsageRecorder::ShouldIgnoreWebState(web::WebState* web_state) {
  // Do not count chrome:// urls to avoid data noise.  For example, if they were
  // counted, every new tab created would add noise to the page load count.
  web::NavigationItem* pending_item =
      web_state->GetNavigationManager()->GetPendingItem();
  if (pending_item)
    return pending_item->GetVirtualURL().SchemeIs(kChromeUIScheme);

  web::NavigationItem* last_committed_item =
      web_state->GetNavigationManager()->GetLastCommittedItem();
  if (last_committed_item)
    return last_committed_item->GetVirtualURL().SchemeIs(kChromeUIScheme);

  return false;
}

bool TabUsageRecorder::WebStateAlreadyEvicted(web::WebState* web_state) {
  auto iter = evicted_web_states_.find(web_state);
  return iter != evicted_web_states_.end();
}

TabUsageRecorder::TabStateWhenSelected TabUsageRecorder::ExtractWebStateState(
    web::WebState* web_state) {
  if (!web_state->IsEvicted())
    return IN_MEMORY;

  auto iter = evicted_web_states_.find(web_state);
  if (iter != evicted_web_states_.end()) {
    TabStateWhenSelected web_state_state = iter->second;
    evicted_web_states_.erase(iter);
    return web_state_state;
  }

  return EVICTED;
}

void TabUsageRecorder::RecordRestoreStartTime() {
  base::TimeTicks time_now = base::TimeTicks::Now();
  // Record the time delta since the last eviction reload was seen.
  LOCAL_HISTOGRAM_TIMES(kTimeBetweenRestores, time_now - restore_start_time_);
  restore_start_time_ = time_now;
  evicted_web_state_reload_start_time_ = time_now;
}

int TabUsageRecorder::GetLiveWebStatesCount() const {
  int count = 0;
  for (int index = 0; index < web_state_list_->count(); ++index) {
    if (!web_state_list_->GetWebStateAt(index)->IsEvicted())
      ++count;
  }
  return count;
}

void TabUsageRecorder::OnWebStateDestroyed(web::WebState* web_state) {
  if (web_state == web_state_created_selected_)
    web_state_created_selected_ = nullptr;

  if (web_state == evicted_web_state_)
    evicted_web_state_ = nullptr;

  if (web_state == mode_switch_web_state_)
    mode_switch_web_state_ = nullptr;

  auto evicted_web_states_iter = evicted_web_states_.find(web_state);
  if (evicted_web_states_iter != evicted_web_states_.end())
    evicted_web_states_.erase(evicted_web_states_iter);

  web_state->RemoveObserver(this);
}

void TabUsageRecorder::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (PageTransitionCoreTypeIs(navigation_context->GetPageTransition(),
                               ui::PAGE_TRANSITION_RELOAD)) {
    RecordReload(web_state);
  }
}

void TabUsageRecorder::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  RecordPageLoadDone(web_state, load_completion_status ==
                                    web::PageLoadCompletionStatus::SUCCESS);
}

void TabUsageRecorder::RenderProcessGone(web::WebState* web_state) {
  bool is_active;
  switch ([UIApplication sharedApplication].applicationState) {
    case UIApplicationStateActive:
      is_active = true;
      break;
    case UIApplicationStateInactive:
    case UIApplicationStateBackground:
      is_active = false;
      break;
  }
  RendererTerminated(web_state, web_state->IsVisible(), is_active);
}

void TabUsageRecorder::WebStateDestroyed(web::WebState* web_state) {
  // TabUsageRecorder only watches WebState inserted in a WebStateList. The
  // WebStateList owns the WebStates it manages. TabUsageRecorder removes
  // itself from WebStates' WebStateObservers when notified by WebStateList
  // that a WebState is removed, so it should never notice WebStateDestroyed
  // event. Thus the implementation enforces this with NOTREACHED().
  NOTREACHED();
}

void TabUsageRecorder::WebStateInsertedAt(WebStateList* web_state_list,
                                          web::WebState* web_state,
                                          int index,
                                          bool activating) {
  if (activating)
    web_state_created_selected_ = web_state;

  web_state->AddObserver(this);
}

void TabUsageRecorder::WebStateReplacedAt(WebStateList* web_state_list,
                                          web::WebState* old_web_state,
                                          web::WebState* new_web_state,
                                          int index) {
  OnWebStateDestroyed(old_web_state);

  if (new_web_state)
    new_web_state->AddObserver(this);
}

void TabUsageRecorder::WebStateDetachedAt(WebStateList* web_state_list,
                                          web::WebState* web_state,
                                          int index) {
  OnWebStateDestroyed(web_state);
}

void TabUsageRecorder::WebStateActivatedAt(WebStateList* web_state_list,
                                           web::WebState* old_web_state,
                                           web::WebState* new_web_state,
                                           int active_index,
                                           int reason) {
  if (reason & WebStateListObserver::CHANGE_REASON_USER_ACTION)
    RecordTabSwitched(old_web_state, new_web_state);
}

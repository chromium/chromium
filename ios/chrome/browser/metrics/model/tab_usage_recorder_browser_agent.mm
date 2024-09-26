// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/tab_usage_recorder_browser_agent.h"

#import <UIKit/UIKit.h>

#import "base/metrics/histogram_macros.h"
#import "components/previous_session_info/previous_session_info.h"
#import "components/ukm/ios/ukm_url_recorder.h"
#import "ios/chrome/browser/prerender/model/prerender_service.h"
#import "ios/chrome/browser/prerender/model/prerender_service_factory.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/components/webui/web_ui_url_constants.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "services/metrics/public/cpp/ukm_builders.h"
#import "ui/base/page_transition_types.h"

BROWSER_USER_DATA_KEY_IMPL(TabUsageRecorderBrowserAgent)

TabUsageRecorderBrowserAgent::TabUsageRecorderBrowserAgent(Browser* browser)
    : restore_start_time_(base::TimeTicks::Now()),
      web_state_list_(browser->GetWebStateList()),
      prerender_service_(
          PrerenderServiceFactory::GetForProfile(browser->GetProfile())) {
  browser->AddObserver(this);

  DCHECK(web_state_list_);
  web_state_list_->AddObserver(this);
  for (int index = 0; index < web_state_list_->count(); ++index) {
    web::WebState* web_state = web_state_list_->GetWebStateAt(index);
    web_state->AddObserver(this);
  }

  ProfileIOS* profile = browser->GetProfile();
  session_restoration_service_observation_.Observe(
      SessionRestorationServiceFactory::GetForProfile(profile));

  // Register for backgrounding and foregrounding notifications. It is safe for
  // the block to capture a pointer to `this` as they are unregistered in the
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

TabUsageRecorderBrowserAgent::~TabUsageRecorderBrowserAgent() {
  DCHECK(!application_foregrounding_observer_);
  DCHECK(!application_backgrounding_observer_);
}

void TabUsageRecorderBrowserAgent::BrowserDestroyed(Browser* browser) {
  DCHECK_EQ(browser->GetWebStateList(), web_state_list_);
  for (int index = 0; index < web_state_list_->count(); ++index) {
    web::WebState* web_state = web_state_list_->GetWebStateAt(index);
    web_state->RemoveObserver(this);
  }

  web_state_list_->RemoveObserver(this);
  browser->RemoveObserver(this);
  session_restoration_service_observation_.Reset();
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
  web_state_list_ = nullptr;
}

void TabUsageRecorderBrowserAgent::InitialRestoredTabs(
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
      evicted_web_states_[web_state] =
          tab_usage_recorder::EVICTED_DUE_TO_COLD_START;
    }
  }
}

void TabUsageRecorderBrowserAgent::RecordTabSwitched(
    web::WebState* old_web_state,
    web::WebState* new_web_state) {
  // If a tab was created to be selected, and is selected shortly thereafter,
  // it should not add its state to the "kSelectedTabHistogramName" metric.
  // `web_state_created_selected_` is reset at the first tab switch seen after
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

  ResetEvictedTab();

  if (ShouldIgnoreWebState(new_web_state) || was_just_created)
    return;

  // Should never happen.  Keeping the check to ensure that the prerender logic
  // is never overlooked, should behavior at the tab_model level change.
  DCHECK(!prerender_service_ ||
         !prerender_service_->IsWebStatePrerendered(new_web_state));

  tab_usage_recorder::TabStateWhenSelected web_state_state =
      ExtractWebStateState(new_web_state);
  if (web_state_state != tab_usage_recorder::IN_MEMORY) {
    // Keep track of the current 'evicted' tab.
    evicted_web_state_ = new_web_state;
    evicted_web_state_state_ = web_state_state;
    UMA_HISTOGRAM_COUNTS_1M(
        tab_usage_recorder::kPageLoadsBeforeEvictedTabSelected, page_loads_);
    ResetPageLoads();
  }

  UMA_HISTOGRAM_ENUMERATION(tab_usage_recorder::kSelectedTabHistogramName,
                            web_state_state,
                            tab_usage_recorder::TAB_STATE_COUNT);
}

void TabUsageRecorderBrowserAgent::RecordPrimaryBrowserChange(
    bool primary_browser) {
  web::WebState* active_web_state =
      web_state_list_ ? web_state_list_->GetActiveWebState() : nullptr;
  if (primary_browser) {
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

void TabUsageRecorderBrowserAgent::RecordPageLoadStart(
    web::WebState* web_state) {
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
    ResetEvictedTab();
  }
}

void TabUsageRecorderBrowserAgent::RecordPageLoadDone(
    web::WebState* web_state) {
  if (!web_state)
    return;
  if (web_state == evicted_web_state_) {
    ResetEvictedTab();
  }
}

void TabUsageRecorderBrowserAgent::RecordReload(web::WebState* web_state) {
  if (!ShouldIgnoreWebState(web_state)) {
    page_loads_++;
  }
}

void TabUsageRecorderBrowserAgent::RendererTerminated(
    web::WebState* terminated_web_state,
    bool web_state_visible,
    bool application_active) {
  // Log the tab state for the termination.
  const tab_usage_recorder::RendererTerminationTabState web_state_state =
      application_active
          ? (web_state_visible
                 ? tab_usage_recorder::FOREGROUND_TAB_FOREGROUND_APP
                 : tab_usage_recorder::BACKGROUND_TAB_FOREGROUND_APP)
          : (web_state_visible
                 ? tab_usage_recorder::FOREGROUND_TAB_BACKGROUND_APP
                 : tab_usage_recorder::BACKGROUND_TAB_BACKGROUND_APP);

  UMA_HISTOGRAM_ENUMERATION(
      tab_usage_recorder::kRendererTerminationStateHistogram,
      static_cast<int>(web_state_state),
      static_cast<int>(tab_usage_recorder::TERMINATION_TAB_STATE_COUNT));
  if (!web_state_visible) {
    if (WebStateAlreadyEvicted(terminated_web_state)) {
      // A web state may get notified multiple times that it's been evicted.
      // To avoid double-counting, don't do any further processing if this
      // happens.
      return;
    }
    evicted_web_states_[terminated_web_state] =
        tab_usage_recorder::EVICTED_DUE_TO_RENDERER_TERMINATION;
  }
  base::TimeTicks now = base::TimeTicks::Now();
  termination_timestamps_.push_back(now);

  // Log if a memory warning was seen recently.
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  BOOL saw_memory_warning =
      [defaults boolForKey:previous_session_info_constants::
                               kDidSeeMemoryWarningShortlyBeforeTerminating];

  // Log number of live tabs after the renderer termination. This count does not
  // include `terminated_web_state`.
  int live_web_states_count = GetLiveWebStatesCount();
  UMA_HISTOGRAM_COUNTS_100(
      tab_usage_recorder::kRendererTerminationAliveRenderers,
      live_web_states_count);

  UMA_HISTOGRAM_COUNTS_100(
      tab_usage_recorder::kRendererTerminationTotalTabCount,
      web_state_list_->count());

  // Clear `termination_timestamps_` of timestamps older than
  // `kSecondsBeforeRendererTermination` ago.
  base::TimeDelta seconds_before =
      base::Seconds(tab_usage_recorder::kSecondsBeforeRendererTermination);
  base::TimeTicks timestamp_boundary = now - seconds_before;
  while (termination_timestamps_.front() < timestamp_boundary) {
    termination_timestamps_.pop_front();
  }

  // Log number of recently alive tabs, where recently alive is defined to mean
  // alive within the past `kSecondsBeforeRendererTermination`.
  NSUInteger recently_live_web_states_count =
      live_web_states_count + termination_timestamps_.size();
  UMA_HISTOGRAM_COUNTS_100(
      tab_usage_recorder::kRendererTerminationRecentlyAliveRenderers,
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

void TabUsageRecorderBrowserAgent::AppDidEnterBackground() {
  base::TimeTicks time_now = base::TimeTicks::Now();
  LOCAL_HISTOGRAM_TIMES(tab_usage_recorder::kTimeAfterLastRestore,
                        time_now - restore_start_time_);
  if (evicted_web_state_ &&
      evicted_web_state_reload_start_time_ != base::TimeTicks()) {
    ResetEvictedTab();
  }
}

void TabUsageRecorderBrowserAgent::AppWillEnterForeground() {
  restore_start_time_ = base::TimeTicks::Now();
}

void TabUsageRecorderBrowserAgent::ResetPageLoads() {
  page_loads_ = 0;
}

int TabUsageRecorderBrowserAgent::EvictedTabsMapSize() {
  return evicted_web_states_.size();
}

void TabUsageRecorderBrowserAgent::ResetAll() {
  ResetEvictedTab();
  ResetPageLoads();
  evicted_web_states_.clear();
}

void TabUsageRecorderBrowserAgent::ResetEvictedTab() {
  evicted_web_state_ = nullptr;
  evicted_web_state_state_ = tab_usage_recorder::IN_MEMORY;
  evicted_web_state_reload_start_time_ = base::TimeTicks();
}

bool TabUsageRecorderBrowserAgent::ShouldIgnoreWebState(
    web::WebState* web_state) {
  // Do not count chrome:// urls to avoid data noise.  For example, if they were
  // counted, every new tab created would add noise to the page load count.
  return web_state->GetVisibleURL().SchemeIs(kChromeUIScheme);
}

bool TabUsageRecorderBrowserAgent::WebStateAlreadyEvicted(
    web::WebState* web_state) {
  auto iter = evicted_web_states_.find(web_state);
  return iter != evicted_web_states_.end();
}

tab_usage_recorder::TabStateWhenSelected
TabUsageRecorderBrowserAgent::ExtractWebStateState(web::WebState* web_state) {
  if (!web_state->IsEvicted())
    return tab_usage_recorder::IN_MEMORY;

  auto iter = evicted_web_states_.find(web_state);
  if (iter != evicted_web_states_.end()) {
    tab_usage_recorder::TabStateWhenSelected web_state_state = iter->second;
    evicted_web_states_.erase(iter);
    return web_state_state;
  }

  return tab_usage_recorder::EVICTED;
}

void TabUsageRecorderBrowserAgent::RecordRestoreStartTime() {
  base::TimeTicks time_now = base::TimeTicks::Now();
  // Record the time delta since the last eviction reload was seen.
  LOCAL_HISTOGRAM_TIMES(tab_usage_recorder::kTimeBetweenRestores,
                        time_now - restore_start_time_);
  restore_start_time_ = time_now;
  evicted_web_state_reload_start_time_ = time_now;
}

int TabUsageRecorderBrowserAgent::GetLiveWebStatesCount() const {
  int count = 0;
  for (int index = 0; index < web_state_list_->count(); ++index) {
    if (!web_state_list_->GetWebStateAt(index)->IsEvicted())
      ++count;
  }
  return count;
}

void TabUsageRecorderBrowserAgent::OnWebStateDestroyed(
    web::WebState* web_state) {
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

bool TabUsageRecorderBrowserAgent::IsTransitionBetweenDesktopAndMobileUserAgent(
    web::UserAgentType agent_type,
    web::UserAgentType other_agent_type) {
  if (agent_type == web::UserAgentType::NONE)
    return false;

  if (other_agent_type == web::UserAgentType::NONE)
    return false;

  return agent_type != other_agent_type;
}

bool TabUsageRecorderBrowserAgent::ShouldRecordPageLoadStartForNavigation(
    web::NavigationContext* navigation) {
  web::NavigationManager* navigation_manager =
      navigation->GetWebState()->GetNavigationManager();

  web::NavigationItem* last_committed_item =
      navigation_manager->GetLastCommittedItem();
  if (!last_committed_item) {
    // Opening a child window and loading URL there.
    // http://crbug.com/773160
    return false;
  }

  web::NavigationItem* pending_item = navigation_manager->GetPendingItem();
  if (pending_item) {
    if (IsTransitionBetweenDesktopAndMobileUserAgent(
            pending_item->GetUserAgentType(),
            last_committed_item->GetUserAgentType())) {
      // Switching between Desktop and Mobile user agent.
      return false;
    }
  }

  ui::PageTransition transition = navigation->GetPageTransition();
  if (!ui::PageTransitionIsNewNavigation(transition)) {
    // Back/forward navigation or reload.
    return false;
  }

  if ((transition & ui::PAGE_TRANSITION_CLIENT_REDIRECT) != 0) {
    // Client redirect.
    return false;
  }

  static const ui::PageTransition kRecordedPageTransitionTypes[] = {
      ui::PAGE_TRANSITION_TYPED,
      ui::PAGE_TRANSITION_LINK,
      ui::PAGE_TRANSITION_GENERATED,
      ui::PAGE_TRANSITION_AUTO_BOOKMARK,
      ui::PAGE_TRANSITION_FORM_SUBMIT,
      ui::PAGE_TRANSITION_KEYWORD,
      ui::PAGE_TRANSITION_KEYWORD_GENERATED,
  };

  for (size_t i = 0; i < std::size(kRecordedPageTransitionTypes); ++i) {
    const ui::PageTransition recorded_type = kRecordedPageTransitionTypes[i];
    if (ui::PageTransitionCoreTypeIs(transition, recorded_type)) {
      return true;
    }
  }

  return false;
}

#pragma mark - WebStateObserver

void TabUsageRecorderBrowserAgent::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (PageTransitionCoreTypeIs(navigation_context->GetPageTransition(),
                               ui::PAGE_TRANSITION_RELOAD)) {
    RecordReload(web_state);
  }
  if (ShouldRecordPageLoadStartForNavigation(navigation_context)) {
    RecordPageLoadStart(web_state);
  }
}

void TabUsageRecorderBrowserAgent::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  RecordPageLoadDone(web_state);
}

void TabUsageRecorderBrowserAgent::RenderProcessGone(web::WebState* web_state) {
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

void TabUsageRecorderBrowserAgent::WebStateDestroyed(web::WebState* web_state) {
  // TabUsageRecorder only watches WebState inserted in a WebStateList. The
  // WebStateList owns the WebStates it manages. TabUsageRecorder removes
  // itself from WebStates' WebStateObservers when notified by WebStateList
  // that a WebState is removed, so it should never notice WebStateDestroyed
  // event. Thus the implementation enforces this with NOTREACHED().
  NOTREACHED_IN_MIGRATION();
}

#pragma mark - WebStateListObserver

void TabUsageRecorderBrowserAgent::WebStateListDidChange(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateListStatus& status) {
  switch (change.type()) {
    case WebStateListChange::Type::kStatusOnly:
      // The activation is handled after this switch statement.
      break;
    case WebStateListChange::Type::kDetach: {
      const WebStateListChangeDetach& detach_change =
          change.As<WebStateListChangeDetach>();
      OnWebStateDestroyed(detach_change.detached_web_state());
      break;
    }
    case WebStateListChange::Type::kMove:
      // Do nothing when a WebState is moved.
      break;
    case WebStateListChange::Type::kReplace: {
      const WebStateListChangeReplace& replace_change =
          change.As<WebStateListChangeReplace>();
      OnWebStateDestroyed(replace_change.replaced_web_state());
      replace_change.inserted_web_state()->AddObserver(this);
      break;
    }
    case WebStateListChange::Type::kInsert: {
      const WebStateListChangeInsert& insert_change =
          change.As<WebStateListChangeInsert>();
      web::WebState* inserted_web_state = insert_change.inserted_web_state();
      if (status.active_web_state_change()) {
        web_state_created_selected_ = inserted_web_state;
      }

      inserted_web_state->AddObserver(this);
      break;
    }
    case WebStateListChange::Type::kGroupCreate:
      // Do nothing when a group is created.
      break;
    case WebStateListChange::Type::kGroupVisualDataUpdate:
      // Do nothing when a tab group's visual data are updated.
      break;
    case WebStateListChange::Type::kGroupMove:
      // Do nothing when a tab group is moved.
      break;
    case WebStateListChange::Type::kGroupDelete:
      // Do nothing when a group is deleted.
      break;
  }

  if (status.active_web_state_change() &&
      change.type() != WebStateListChange::Type::kReplace) {
    RecordTabSwitched(status.old_active_web_state, status.new_active_web_state);
  }
}

#pragma mark - SessionRestorationObserver

void TabUsageRecorderBrowserAgent::WillStartSessionRestoration(
    Browser* browser) {
  // Nothing to do.
}

void TabUsageRecorderBrowserAgent::SessionRestorationFinished(
    Browser* browser,
    const std::vector<web::WebState*>& restored_web_states) {
  // Ignore the event if it does not correspond to the browser this
  // object is bound to (which can happen with the optimised session
  // storage code).
  if (browser->GetWebStateList() != web_state_list_) {
    return;
  }

  InitialRestoredTabs(web_state_list_->GetActiveWebState(),
                      restored_web_states);
}

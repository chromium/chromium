// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_MODEL_TAB_USAGE_RECORDER_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_METRICS_MODEL_TAB_USAGE_RECORDER_BROWSER_AGENT_H_

#import <map>
#import <memory>
#import <vector>

#import "base/containers/circular_deque.h"
#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
#import "base/time/time.h"
#import "ios/chrome/browser/metrics/model/tab_usage_recorder_metrics.h"
#import "ios/chrome/browser/sessions/model/session_restoration_observer.h"
#import "ios/chrome/browser/shared/model/browser/browser_observer.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"
#import "ios/web/common/user_agent.h"
#import "ios/web/public/web_state_observer.h"

class PrerenderService;
class SessionRestorationService;
class WebStateList;

namespace web {
class WebState;
}

// Reports usage about the lifecycle of a single Browser's tabs.
class TabUsageRecorderBrowserAgent
    : public BrowserObserver,
      public BrowserUserData<TabUsageRecorderBrowserAgent>,
      public web::WebStateObserver,
      public WebStateListObserver,
      public SessionRestorationObserver {
 public:
  // Not copyable or moveable
  TabUsageRecorderBrowserAgent(const TabUsageRecorderBrowserAgent&) = delete;
  TabUsageRecorderBrowserAgent& operator=(const TabUsageRecorderBrowserAgent&) =
      delete;

  ~TabUsageRecorderBrowserAgent() override;

  // Called during startup when the tab model is created, or shortly after a
  // post-crash launch if the tabs are restored.  `web_states` is an array
  // containing/ the tabs being restored in the current tab model.
  // `active_web_state` is the tab currently in the foreground.
  void InitialRestoredTabs(web::WebState* active_web_state,
                           const std::vector<web::WebState*>& web_states);

  // Called when a tab switch is made.  Determines what value to record, and
  // when to reset the page load counter.
  void RecordTabSwitched(web::WebState* old_web_state,
                         web::WebState* new_web_state);

  // Called when the Browser which the user is primarily interacting with has
  // changed. If the user began interacting with `active_web_state`,
  // `primary_browser` should be true. If the user stopped interacting with
  // `active_web_state`, `primary_browser` should be false.
  void RecordPrimaryBrowserChange(bool primary_browser);

  // Called when a page load begins, to keep track of how many page loads
  // happen before an evicted tab is seen.
  void RecordPageLoadStart(web::WebState* web_state);

  // Called when a page load finishes, to track the load time for evicted tabs.
  void RecordPageLoadDone(web::WebState* web_state);

  // Called when there is a user-initiated reload.
  void RecordReload(web::WebState* web_state);

  // Called when WKWebView's renderer is terminated. `tab` contains the tab
  // whose renderer was terminated, `tab_visible` indicates whether or not
  // the tab was visible when the renderer terminated and `application_active`
  // indicates whether the application was in the foreground or background.
  void RendererTerminated(web::WebState* web_state,
                          bool web_state_visible,
                          bool application_active);

  // Called when the app has been backgrounded.
  void AppDidEnterBackground();

  // Called when the app has been foregrounded.
  void AppWillEnterForeground();

  // Resets the page load count.
  void ResetPageLoads();

  // Size of `evicted_web_states_`.  Used for testing.
  int EvictedTabsMapSize();

  // Resets all tracked data.  Used for testing.
  void ResetAll();

 private:
  // TODO(crbug.com/41324440): remove this once the code has been refactored not
  // to depends on injecting values in `termination_timestamps_`.
  friend class TabUsageRecorderBrowserAgentTest;

  friend class BrowserUserData<TabUsageRecorderBrowserAgent>;
  BROWSER_USER_DATA_KEY_DECL();

  explicit TabUsageRecorderBrowserAgent(Browser* browser);

  // BrowserObserver methods
  void BrowserDestroyed(Browser* browser) override;

  // Clear out all state regarding a current evicted tab.
  void ResetEvictedTab();

  // Whether or not a tab can be disregarded by the metrics.
  bool ShouldIgnoreWebState(web::WebState* web_state);

  // Whether or not the tab has already been evicted.
  bool WebStateAlreadyEvicted(web::WebState* web_state);

  // Returns the state of the given tab.  Call only once per tab, as it removes
  // the tab from `evicted_web_states_`.
  tab_usage_recorder::TabStateWhenSelected ExtractWebStateState(
      web::WebState* web_state);

  // Records various time metrics when a restore of an evicted tab begins.
  void RecordRestoreStartTime();

  // Returns the number of WebState that are still alive (in-memory).
  int GetLiveWebStatesCount() const;

  // Called before one of the tracked WebState is destroyed. The WebState is
  // still valid but will become invalid afterwards, so any reference to it
  // should be removed.
  void OnWebStateDestroyed(web::WebState* web_state);

  // Returns whether `agent_type` and `other_agent_type` are different user
  // agent types. If either of them is web::UserAgentType::NONE, then return
  // false.
  bool IsTransitionBetweenDesktopAndMobileUserAgent(
      web::UserAgentType agent_type,
      web::UserAgentType other_agent_type);

  // Returns whether RecordPageLoadStart should be called for the given
  // navigation.
  bool ShouldRecordPageLoadStartForNavigation(
      web::NavigationContext* navigation);

  // web::WebStateObserver implementation.
  void DidStartNavigation(web::WebState* web_state,
                          web::NavigationContext* navigation_context) override;
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;
  void RenderProcessGone(web::WebState* web_state) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // WebStateListObserver implementation.
  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) override;

  // SessionRestorationObserver implementation.
  void WillStartSessionRestoration(Browser* browser) override;
  void SessionRestorationFinished(
      Browser* browser,
      const std::vector<web::WebState*>& restored_web_states) override;

  // Keep track of when the most recent tab restore begins, to record the time
  // between evicted-tab-reloads.
  base::TimeTicks restore_start_time_;

  // Keep track of the timestamps of renderer terminations in order to find the
  // number of recently alive tabs when a renderer termination occurs.
  base::circular_deque<base::TimeTicks> termination_timestamps_;

  // Number of page loads since the last evicted tab was seen.
  unsigned int page_loads_ = 0;

  // Keep track of the current tab, but only if it has been evicted.
  // This is kept as a pointer value only - it should never be dereferenced.
  raw_ptr<web::WebState> evicted_web_state_ = nullptr;

  // State of `evicted_web_state_` at the time it became the current tab.
  tab_usage_recorder::TabStateWhenSelected evicted_web_state_state_ =
      tab_usage_recorder::IN_MEMORY;

  // Keep track of the tab last selected when this tab model was switched
  // away from to another mode (e.g. to incognito).
  // Kept as a pointer value only - it should never be dereferenced.
  raw_ptr<web::WebState> mode_switch_web_state_ = nullptr;

  // Keep track of a tab that was created to be immediately selected.  It should
  // not contribute to the "StatusWhenSwitchedBackToForeground" metric.
  raw_ptr<web::WebState> web_state_created_selected_ = nullptr;

  // Keep track of when the evicted tab starts to reload, so that the total
  // time it takes to reload can be recorded.
  base::TimeTicks evicted_web_state_reload_start_time_;

  // Keep track of the tabs that have a known eviction cause.
  std::map<web::WebState*, tab_usage_recorder::TabStateWhenSelected>
      evicted_web_states_;

  // The WebStateList containing all the monitored tabs.
  raw_ptr<WebStateList> web_state_list_;  // weak

  // The PrerenderService used to check whether a tab is pre-rendering. May
  // be null during unit testing.
  raw_ptr<PrerenderService> prerender_service_;

  // Observation for SessionRestorationService events.
  base::ScopedObservation<SessionRestorationService, SessionRestorationObserver>
      session_restoration_service_observation_{this};

  // Observers for NSNotificationCenter notifications.
  __strong id<NSObject> application_backgrounding_observer_;
  __strong id<NSObject> application_foregrounding_observer_;
};

#endif  // IOS_CHROME_BROWSER_METRICS_MODEL_TAB_USAGE_RECORDER_BROWSER_AGENT_H_

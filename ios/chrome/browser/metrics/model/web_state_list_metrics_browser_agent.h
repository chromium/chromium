// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_MODEL_WEB_STATE_LIST_METRICS_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_METRICS_MODEL_WEB_STATE_LIST_METRICS_BROWSER_AGENT_H_

#import "ios/chrome/browser/sessions/session_restoration_observer.h"
#import "ios/chrome/browser/shared/model/browser/browser_observer.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"
#import "ios/web/public/web_state_observer.h"

class AllWebStateObservationForwarder;
class SessionMetrics;

class WebStateListMetricsBrowserAgent
    : public BrowserObserver,
      public WebStateListObserver,
      public SessionRestorationObserver,
      public web::WebStateObserver,
      public BrowserUserData<WebStateListMetricsBrowserAgent> {
 public:
  WebStateListMetricsBrowserAgent(const WebStateListMetricsBrowserAgent&) =
      delete;
  WebStateListMetricsBrowserAgent& operator=(
      const WebStateListMetricsBrowserAgent&) = delete;

  ~WebStateListMetricsBrowserAgent() override;

 private:
  friend class BrowserUserData<WebStateListMetricsBrowserAgent>;
  BROWSER_USER_DATA_KEY_DECL();

  WebStateListMetricsBrowserAgent(Browser* browser,
                                  SessionMetrics* session_metrics);

  // BrowserObserver methods
  void BrowserDestroyed(Browser* browser) override;

  // SessionRestorationObserver implementation.
  void WillStartSessionRestoration(Browser* browser) override;
  void SessionRestorationFinished(
      Browser* browser,
      const std::vector<web::WebState*>& restored_web_states) override;

  // web::WebStateObserver
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;

  // WebStateListObserver:
  void WebStateListWillChange(WebStateList* web_state_list,
                              const WebStateListChangeDetach& detach_change,
                              const WebStateListStatus& status) override;
  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) override;

  // The WebStateList containing all the monitored tabs.
  WebStateList* web_state_list_ = nullptr;

  // The object storing the metrics.
  SessionMetrics* session_metrics_ = nullptr;

  // Whether metric recording is paused (for session restoration).
  bool metric_collection_paused_ = false;

  std::unique_ptr<AllWebStateObservationForwarder> web_state_forwarder_;
};

#endif  // IOS_CHROME_BROWSER_METRICS_MODEL_WEB_STATE_LIST_METRICS_BROWSER_AGENT_H_

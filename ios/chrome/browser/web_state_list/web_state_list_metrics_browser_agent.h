// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_STATE_LIST_WEB_STATE_LIST_METRICS_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_WEB_STATE_LIST_WEB_STATE_LIST_METRICS_BROWSER_AGENT_H_

#include "base/macros.h"
#import "ios/chrome/browser/main/browser_observer.h"
#import "ios/chrome/browser/main/browser_user_data.h"
#include "ios/chrome/browser/sessions/session_restoration_observer.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer.h"

class WebStateListMetricsBrowserAgent
    : BrowserObserver,
      public WebStateListObserver,
      public SessionRestorationObserver,
      public BrowserUserData<WebStateListMetricsBrowserAgent> {
 public:
  WebStateListMetricsBrowserAgent();
  ~WebStateListMetricsBrowserAgent() override;

  void RecordSessionMetrics();

  // WebStateListObserver implementation.
  void WebStateInsertedAt(WebStateList* web_state_list,
                          web::WebState* web_state,
                          int index,
                          bool activating) override;
  void WebStateDetachedAt(WebStateList* web_state_list,
                          web::WebState* web_state,
                          int index) override;
  void WebStateActivatedAt(WebStateList* web_state_list,
                           web::WebState* old_web_state,
                           web::WebState* new_web_state,
                           int active_index,
                           ActiveWebStateChangeReason reason) override;

 private:
  explicit WebStateListMetricsBrowserAgent(Browser* browser);
  friend class BrowserUserData<WebStateListMetricsBrowserAgent>;
  BROWSER_USER_DATA_KEY_DECL();

  // BrowserObserver methods
  void BrowserDestroyed(Browser* browser) override;

  // Reset metrics counters.
  void ResetSessionMetrics();

  // SessionRestorationObserver implementation.
  void WillStartSessionRestoration() override;
  void SessionRestorationFinished(
      const std::vector<web::WebState*>& restored_web_states) override;

  // The WebStateList containing all the monitored tabs.
  WebStateList* web_state_list_;  // weak

  // Counters for metrics.
  int inserted_web_state_counter_ = 0;
  int detached_web_state_counter_ = 0;
  int activated_web_state_counter_ = 0;
  bool metric_collection_paused_ = false;

  DISALLOW_COPY_AND_ASSIGN(WebStateListMetricsBrowserAgent);
};

#endif  // IOS_CHROME_BROWSER_WEB_STATE_LIST_WEB_STATE_LIST_METRICS_BROWSER_AGENT_H_

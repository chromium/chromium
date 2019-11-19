// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_STATE_LIST_WEB_STATE_LIST_METRICS_OBSERVER_H_
#define IOS_CHROME_BROWSER_WEB_STATE_LIST_WEB_STATE_LIST_METRICS_OBSERVER_H_

#include "base/macros.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer.h"

class WebStateListMetricsObserver : public WebStateListObserver {
 public:
  WebStateListMetricsObserver();
  ~WebStateListMetricsObserver() override;

  void RecordSessionMetrics();

  // TODO(crbug.com/1010164): Don't define these methods here. Instead implement
  // SessionRestorationObserver methods.
  void WillStartSessionRestoration();
  void SessionRestorationFinished();

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
                           int reason) override;

 private:
  // Counters for metrics.
  int inserted_web_state_counter_;
  int detached_web_state_counter_;
  int activated_web_state_counter_;
  bool metric_collection_paused_;

  // Reset metrics counters.
  void ResetSessionMetrics();

  DISALLOW_COPY_AND_ASSIGN(WebStateListMetricsObserver);
};

#endif  // IOS_CHROME_BROWSER_WEB_STATE_LIST_WEB_STATE_LIST_METRICS_OBSERVER_H_

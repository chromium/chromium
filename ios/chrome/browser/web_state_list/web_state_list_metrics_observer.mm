// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_state_list/web_state_list_metrics_observer.h"

#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

WebStateListMetricsObserver::WebStateListMetricsObserver() {
  ResetSessionMetrics();
}

WebStateListMetricsObserver::~WebStateListMetricsObserver() = default;

void WebStateListMetricsObserver::RecordSessionMetrics() {
  UMA_HISTOGRAM_CUSTOM_COUNTS("Session.ClosedTabCounts",
                              detached_web_state_counter_, 1, 200, 50);
  UMA_HISTOGRAM_CUSTOM_COUNTS("Session.OpenedTabCounts",
                              activated_web_state_counter_, 1, 200, 50);
  UMA_HISTOGRAM_CUSTOM_COUNTS("Session.NewTabCounts",
                              inserted_web_state_counter_, 1, 200, 50);
  ResetSessionMetrics();
}

void WebStateListMetricsObserver::WillStartSessionRestoration() {
  metric_collection_paused_ = true;
}

void WebStateListMetricsObserver::SessionRestorationFinished() {
  metric_collection_paused_ = false;
}

void WebStateListMetricsObserver::WebStateInsertedAt(
    WebStateList* web_state_list,
    web::WebState* web_state,
    int index,
    bool activating) {
  if (metric_collection_paused_)
    return;
  base::RecordAction(base::UserMetricsAction("MobileNewTabOpened"));
  ++inserted_web_state_counter_;
}

void WebStateListMetricsObserver::WebStateDetachedAt(
    WebStateList* web_state_list,
    web::WebState* web_state,
    int index) {
  if (metric_collection_paused_)
    return;
  base::RecordAction(base::UserMetricsAction("MobileTabClosed"));
  ++detached_web_state_counter_;
}

void WebStateListMetricsObserver::WebStateActivatedAt(
    WebStateList* web_state_list,
    web::WebState* old_web_state,
    web::WebState* new_web_state,
    int active_index,
    int reason) {
  if (metric_collection_paused_)
    return;
  ++activated_web_state_counter_;
  if (!(reason & WebStateListObserver::CHANGE_REASON_USER_ACTION))
    return;

  base::RecordAction(base::UserMetricsAction("MobileTabSwitched"));
}

void WebStateListMetricsObserver::ResetSessionMetrics() {
  inserted_web_state_counter_ = 0;
  detached_web_state_counter_ = 0;
  activated_web_state_counter_ = 0;
  metric_collection_paused_ = false;
}

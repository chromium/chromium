// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/web_performance_metrics/web_performance_metrics_tab_helper.h"

WebPerformanceMetricsTabHelper::WebPerformanceMetricsTabHelper(
    web::WebState* web_state) {
  web_state_observation_.Observe(web_state);
}

WebPerformanceMetricsTabHelper::~WebPerformanceMetricsTabHelper() = default;

void WebPerformanceMetricsTabHelper::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  SetAggregateAbsoluteFirstContentfulPaint(std::numeric_limits<double>::max());
  SetFirstInputDelayLoggingStatus(false);
  has_been_hidden_since_navigation_started_ = !web_state->IsVisible();
}

void WebPerformanceMetricsTabHelper::WasHidden(web::WebState* web_state) {
  has_been_hidden_since_navigation_started_ = true;
}

void WebPerformanceMetricsTabHelper::WebStateDestroyed(
    web::WebState* web_state) {
  web_state_observation_.Reset();
}

double
WebPerformanceMetricsTabHelper::GetAggregateAbsoluteFirstContentfulPaint()
    const {
  return aggregate_absolute_first_contentful_paint_;
}

void WebPerformanceMetricsTabHelper::SetAggregateAbsoluteFirstContentfulPaint(
    double absolute_first_contentful_paint) {
  aggregate_absolute_first_contentful_paint_ = absolute_first_contentful_paint;
}

bool WebPerformanceMetricsTabHelper::GetFirstInputDelayLoggingStatus() const {
  return first_input_delay_has_been_logged;
}

bool WebPerformanceMetricsTabHelper::HasBeenHiddenSinceNavigationStarted()
    const {
  return has_been_hidden_since_navigation_started_;
}

void WebPerformanceMetricsTabHelper::SetFirstInputDelayLoggingStatus(
    bool first_input_delay_logging_status) {
  first_input_delay_has_been_logged = first_input_delay_logging_status;
}

WEB_STATE_USER_DATA_KEY_IMPL(WebPerformanceMetricsTabHelper)

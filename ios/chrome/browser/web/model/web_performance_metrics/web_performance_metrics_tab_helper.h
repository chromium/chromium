// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MODEL_WEB_PERFORMANCE_METRICS_WEB_PERFORMANCE_METRICS_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_WEB_MODEL_WEB_PERFORMANCE_METRICS_WEB_PERFORMANCE_METRICS_TAB_HELPER_H_

#include <limits>

#include "base/scoped_observation.h"
#include "ios/chrome/browser/web/model/web_performance_metrics/web_performance_metrics_java_script_feature_util.h"
#include "ios/web/public/web_state_observer.h"
#include "ios/web/public/web_state_user_data.h"

namespace web {
class WebState;
}

// A Tab Helper that inherits from the WebStateObserver in order to
// notify the WebPerformanceMetricsJavaScriptFeature that a web page
// navigation event has occurred and signals for the feature to log
// the metrics it has caputered into UMA.
class WebPerformanceMetricsTabHelper
    : public web::WebStateObserver,
      public web::WebStateUserData<WebPerformanceMetricsTabHelper> {
 public:
  WebPerformanceMetricsTabHelper(const WebPerformanceMetricsTabHelper&) =
      delete;
  WebPerformanceMetricsTabHelper& operator=(
      const WebPerformanceMetricsTabHelper&) = delete;

  ~WebPerformanceMetricsTabHelper() override;

  // Returns the absolute first contentful paint time aggregated across iframes.
  double GetAggregateAbsoluteFirstContentfulPaint() const;

  // Sets the absolute first contentful paint time.
  void SetAggregateAbsoluteFirstContentfulPaint(
      double absolute_first_contentful_paint);

  // If the web page has logged its First Input Delay, the function
  // returns `true` otherwise it returns `false`
  bool GetFirstInputDelayLoggingStatus() const;

  // Returns whether the WebState has been hidden at any point since the start
  // of the most recent navigation.
  bool HasBeenHiddenSinceNavigationStarted() const;

  // Sets the boolean variable that indicates whether the First Input Delay
  // has been logged in UMA for the current web page.
  void SetFirstInputDelayLoggingStatus(bool first_input_delay_logging_status);

 private:
  friend class web::WebStateUserData<WebPerformanceMetricsTabHelper>;

  explicit WebPerformanceMetricsTabHelper(web::WebState* web_state);

  // WebStateObserver::
  void WebStateDestroyed(web::WebState* web_state) override;

  void DidStartNavigation(web::WebState* web_state,
                          web::NavigationContext* navigation_context) override;
  void WasHidden(web::WebState* web_state) override;

  // Manages the tab helper's connection to the WebState
  base::ScopedObservation<web::WebState, web::WebStateObserver>
      web_state_observation_{this};

  // Stores the earliest absolute First Contentful Paint across a web page's
  // main and subframes.
  double aggregate_absolute_first_contentful_paint_ =
      std::numeric_limits<double>::max();

  // Stores whether the First Input Delay has been logged to UMA for the
  // current web page
  bool first_input_delay_has_been_logged = false;

  // Stores whether the WebState has been hidden at any point since the most
  // recent navigation started.
  bool has_been_hidden_since_navigation_started_ = false;

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_WEB_MODEL_WEB_PERFORMANCE_METRICS_WEB_PERFORMANCE_METRICS_TAB_HELPER_H_

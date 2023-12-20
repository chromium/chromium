// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MODEL_WEB_PERFORMANCE_METRICS_WEB_PERFORMANCE_METRICS_JAVA_SCRIPT_FEATURE_H_
#define IOS_CHROME_BROWSER_WEB_MODEL_WEB_PERFORMANCE_METRICS_WEB_PERFORMANCE_METRICS_JAVA_SCRIPT_FEATURE_H_

#include <optional>

#include "ios/web/public/js_messaging/java_script_feature.h"

// A feature which captures Web Vitals metrics that determine
// JavaScript injected logic's affect on a user's perception
// of web performance.
class WebPerformanceMetricsJavaScriptFeature : public web::JavaScriptFeature {
 public:
  WebPerformanceMetricsJavaScriptFeature();
  ~WebPerformanceMetricsJavaScriptFeature() override;
  // This feature holds no state. Thus, a single static instance
  // suffices.
  static WebPerformanceMetricsJavaScriptFeature* GetInstance();

  // JavaScriptFeature:
  std::optional<std::string> GetScriptMessageHandlerName() const override;
  void ScriptMessageReceived(web::WebState* web_state,
                             const web::ScriptMessage& message) override;

  // Logs the First Contentful Paint time relative to each frame in UMA.
  void LogRelativeFirstContentfulPaint(double value, bool is_main_frame);

  // Logs the earliest Contentful Paint time across main and sub frames in UMA.
  void LogAggregateFirstContentfulPaint(web::WebState* web_state,
                                        double frameNavigationStartTime,
                                        double relativeFirstContentfulPaint,
                                        bool is_main_frame);

  // Logs the First Input Delay time relative to each frame in UMA.
  void LogRelativeFirstInputDelay(double value,
                                  bool is_main_frame,
                                  bool loaded_from_cache);

  // Logs the First Input Delay time across main and sub frames in UMA.
  void LogAggregateFirstInputDelay(web::WebState* web_state,
                                   double first_input_delay,
                                   bool loaded_from_cache);
};

#endif  // IOS_CHROME_BROWSER_WEB_MODEL_WEB_PERFORMANCE_METRICS_WEB_PERFORMANCE_METRICS_JAVA_SCRIPT_FEATURE_H_

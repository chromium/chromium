// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/values.h"

#include "ios/web/js_features/web_performance_metrics/web_performance_metrics_java_script_feature.h"
#include "ios/web/public/js_messaging/java_script_feature_util.h"
#include "ios/web/public/js_messaging/script_message.h"
#include "ios/web/public/js_messaging/web_frame_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kPerformanceMetricsScript[] = "web_performance_metrics_js";
const char kWebPerformanceMetricsScriptName[] = "WebPerformanceMetricsHandler";
}

namespace web {
WebPerformanceMetricsJavaScriptFeature::WebPerformanceMetricsJavaScriptFeature()
    : JavaScriptFeature(ContentWorld::kAnyContentWorld,
                        {FeatureScript::CreateWithFilename(
                            kPerformanceMetricsScript,
                            FeatureScript::InjectionTime::kDocumentStart,
                            FeatureScript::TargetFrames::kAllFrames)}) {}

WebPerformanceMetricsJavaScriptFeature::
    ~WebPerformanceMetricsJavaScriptFeature() = default;

WebPerformanceMetricsJavaScriptFeature*
WebPerformanceMetricsJavaScriptFeature::GetInstance() {
  static base::NoDestructor<WebPerformanceMetricsJavaScriptFeature> instance;
  return instance.get();
}

absl::optional<std::string>
WebPerformanceMetricsJavaScriptFeature::GetScriptMessageHandlerName() const {
  return kWebPerformanceMetricsScriptName;
}

void WebPerformanceMetricsJavaScriptFeature::ScriptMessageReceived(
    web::WebState* web_state,
    const web::ScriptMessage& message) {
  DCHECK(web_state);

  // Verify that the message is well-formed before using it
  if (!message.body()->is_dict()) {
    return;
  }

  std::string* metric = message.body()->FindStringKey("metric");
  if (!metric || metric->empty()) {
    return;
  }

  absl::optional<double> value = message.body()->FindDoubleKey("value");
  if (!value) {
    return;
  }

  if (*metric == "FirstContentfulPaint" && message.is_main_frame()) {
    UMA_HISTOGRAM_TIMES("IOS.Frame.FirstContentfulPaint.MainFrame",
                        base::Milliseconds(value.value()));
  } else if (*metric == "FirstContentfulPaint") {
    UMA_HISTOGRAM_TIMES("IOS.Frame.FirstContentfulPaint.SubFrame",
                        base::Milliseconds(value.value()));
  }
}

}

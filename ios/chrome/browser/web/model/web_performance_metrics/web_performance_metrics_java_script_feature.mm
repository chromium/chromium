// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/web_performance_metrics/web_performance_metrics_java_script_feature.h"

#import "base/ios/ios_util.h"
#import "base/logging.h"
#import "base/metrics/histogram_functions.h"
#import "base/no_destructor.h"
#import "base/strings/strcat.h"
#import "base/values.h"
#import "ios/chrome/browser/web/model/web_performance_metrics/web_performance_metrics_java_script_feature_util.h"
#import "ios/chrome/browser/web/model/web_performance_metrics/web_performance_metrics_tab_helper.h"
#import "ios/web/public/js_messaging/java_script_feature_util.h"
#import "ios/web/public/js_messaging/script_message.h"

namespace {
const char kPerformanceMetricsScript[] = "web_performance_metrics";
const char kWebPerformanceMetricsScriptName[] = "WebPerformanceMetricsHandler";

// The time range's expected min and max values for FirstContentfulPaint
// histograms.
constexpr base::TimeDelta kTimeRangePaintHistogramMin = base::Milliseconds(10);
constexpr base::TimeDelta kTimeRangePaintHistogramMax = base::Minutes(10);

// Number of buckets for the FirstContentfulPaint histograms.
constexpr int kTimeRangePaintHistogramBucketCount = 100;

// The time range's expected min and max values for FirstInputDelay
// histograms.
constexpr base::TimeDelta kTimeRangeInputDelayHistogramMin =
    base::Milliseconds(1);
constexpr base::TimeDelta kTimeRangeInputDelayHistogramMax = base::Seconds(60);

// Number of buckets for the FirstInputDelay histograms.
constexpr int kTimeRangeInputDelayHistogramBucketCount = 50;

}  // namespace

WebPerformanceMetricsJavaScriptFeature::WebPerformanceMetricsJavaScriptFeature()
    : JavaScriptFeature(web::ContentWorld::kIsolatedWorld,
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

std::optional<std::string>
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

  base::Value::Dict& body_dict = message.body()->GetDict();

  std::string* metric = body_dict.FindString("metric");
  if (!metric || metric->empty()) {
    return;
  }

  std::optional<double> value = body_dict.FindDouble("value");
  if (!value) {
    return;
  }

  if (*metric == "FirstContentfulPaint") {
    std::optional<double> frame_navigation_start_time =
        body_dict.FindDouble("frameNavigationStartTime");
    if (!frame_navigation_start_time) {
      return;
    }

    LogRelativeFirstContentfulPaint(value.value(), message.is_main_frame());
    LogAggregateFirstContentfulPaint(web_state,
                                     frame_navigation_start_time.value(),
                                     value.value(), message.is_main_frame());
  } else if (*metric == "FirstInputDelay") {
    std::optional<bool> loaded_from_cache = body_dict.FindBool("cached");
    if (!loaded_from_cache.has_value()) {
      return;
    }

    LogRelativeFirstInputDelay(value.value(), message.is_main_frame(),
                               loaded_from_cache.value());
    LogAggregateFirstInputDelay(web_state, value.value(),
                                loaded_from_cache.value());
  }
}

void WebPerformanceMetricsJavaScriptFeature::LogRelativeFirstContentfulPaint(
    double value,
    bool is_main_frame) {
  if (is_main_frame) {
    UmaHistogramCustomTimes(
        "IOS.Frame.FirstContentfulPaint.MainFrame", base::Milliseconds(value),
        kTimeRangePaintHistogramMin, kTimeRangePaintHistogramMax,
        kTimeRangePaintHistogramBucketCount);
  } else {
    UmaHistogramCustomTimes(
        "IOS.Frame.FirstContentfulPaint.SubFrame", base::Milliseconds(value),
        kTimeRangePaintHistogramMin, kTimeRangePaintHistogramMax,
        kTimeRangePaintHistogramBucketCount);
  }
}

void WebPerformanceMetricsJavaScriptFeature::LogAggregateFirstContentfulPaint(
    web::WebState* web_state,
    double frame_navigation_start_time,
    double relative_first_contentful_paint,
    bool is_main_frame) {
  WebPerformanceMetricsTabHelper* tab_helper =
      WebPerformanceMetricsTabHelper::FromWebState(web_state);

  if (!tab_helper || tab_helper->HasBeenHiddenSinceNavigationStarted()) {
    return;
  }

  const double aggregate =
      tab_helper->GetAggregateAbsoluteFirstContentfulPaint();

  if (is_main_frame) {
    // Finds the earliest First Contentful Paint time across
    // main and subframes and logs that time to UMA.
    web_performance_metrics::FirstContentfulPaint frame = {
        frame_navigation_start_time, relative_first_contentful_paint,
        web_performance_metrics::CalculateAbsoluteFirstContentfulPaint(
            frame_navigation_start_time, relative_first_contentful_paint)};
    base::TimeDelta aggregate_first_contentful_paint =
        web_performance_metrics::CalculateAggregateFirstContentfulPaint(
            aggregate, frame);

    UmaHistogramCustomTimes(
        "PageLoad.PaintTiming.NavigationToFirstContentfulPaint",
        aggregate_first_contentful_paint, kTimeRangePaintHistogramMin,
        kTimeRangePaintHistogramMax, kTimeRangePaintHistogramBucketCount);
  } else if (aggregate == std::numeric_limits<double>::max()) {
    tab_helper->SetAggregateAbsoluteFirstContentfulPaint(
        web_performance_metrics::CalculateAbsoluteFirstContentfulPaint(
            frame_navigation_start_time, relative_first_contentful_paint));
  }
}

void WebPerformanceMetricsJavaScriptFeature::LogRelativeFirstInputDelay(
    double value,
    bool is_main_frame,
    bool loaded_from_cache) {
  base::TimeDelta delta = base::Milliseconds(value);

  if (is_main_frame) {
    if (!loaded_from_cache) {
      UmaHistogramCustomTimes("IOS.Frame.FirstInputDelay.MainFrame2", delta,
                              kTimeRangeInputDelayHistogramMin,
                              kTimeRangeInputDelayHistogramMax,
                              kTimeRangeInputDelayHistogramBucketCount);
    } else if (loaded_from_cache) {
      UmaHistogramCustomTimes(
          "IOS.Frame.FirstInputDelay.MainFrame.AfterBackForwardCacheRestore2",
          delta, kTimeRangeInputDelayHistogramMin,
          kTimeRangeInputDelayHistogramMax,
          kTimeRangeInputDelayHistogramBucketCount);
    }
  } else {
    if (!loaded_from_cache) {
      UmaHistogramCustomTimes("IOS.Frame.FirstInputDelay.SubFrame2", delta,
                              kTimeRangeInputDelayHistogramMin,
                              kTimeRangeInputDelayHistogramMax,
                              kTimeRangeInputDelayHistogramBucketCount);
    } else if (loaded_from_cache) {
      UmaHistogramCustomTimes(
          "IOS.Frame.FirstInputDelay.SubFrame.AfterBackForwardCacheRestore2",
          delta, kTimeRangeInputDelayHistogramMin,
          kTimeRangeInputDelayHistogramMax,
          kTimeRangeInputDelayHistogramBucketCount);
    }
  }
}

void WebPerformanceMetricsJavaScriptFeature::LogAggregateFirstInputDelay(
    web::WebState* web_state,
    double first_input_delay,
    bool loaded_from_cache) {
  WebPerformanceMetricsTabHelper* tab_helper =
      WebPerformanceMetricsTabHelper::FromWebState(web_state);

  if (!tab_helper || tab_helper->HasBeenHiddenSinceNavigationStarted()) {
    return;
  }

  bool first_input_delay_has_been_logged =
      tab_helper->GetFirstInputDelayLoggingStatus();

  if (!first_input_delay_has_been_logged) {
    base::TimeDelta delta = base::Milliseconds(first_input_delay);
    if (loaded_from_cache) {
      // This is an input metric for WebVitals.FirstInputDelay{2, 3} so should
      // not be deleted while those metrics still exist.
      UmaHistogramCustomTimes("PageLoad.InteractiveTiming.FirstInputDelay."
                              "AfterBackForwardCacheRestore",
                              delta, base::Milliseconds(10), base::Minutes(10),
                              100);
      // This is a version of the above metric that uses the same bucketing as
      // non-iOS platforms.
      UmaHistogramCustomTimes("PageLoad.InteractiveTiming.FirstInputDelay."
                              "AfterBackForwardCacheRestore_iOSFixed",
                              delta, kTimeRangeInputDelayHistogramMin,
                              kTimeRangeInputDelayHistogramMax,
                              kTimeRangeInputDelayHistogramBucketCount);
    } else {
      // This is an input metric for WebVitals.FirstInputDelay{2, 3} so should
      // not be deleted while those metrics still exist.
      UmaHistogramCustomTimes("PageLoad.InteractiveTiming.FirstInputDelay4",
                              delta, base::Milliseconds(10), base::Minutes(10),
                              100);
      // This is a version of the above metric that uses the same bucketing as
      // non-iOS platforms.
      UmaHistogramCustomTimes("PageLoad.InteractiveTiming."
                              "FirstInputDelay4_iOSFixed",
                              delta, kTimeRangeInputDelayHistogramMin,
                              kTimeRangeInputDelayHistogramMax,
                              kTimeRangeInputDelayHistogramBucketCount);
    }
    tab_helper->SetFirstInputDelayLoggingStatus(true);
  }
}

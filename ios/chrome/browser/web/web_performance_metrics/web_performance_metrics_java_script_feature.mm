// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/web_performance_metrics/web_performance_metrics_java_script_feature.h"

#import "base/ios/ios_util.h"
#import "base/logging.h"
#import "base/metrics/histogram_functions.h"
#import "base/no_destructor.h"
#import "base/strings/strcat.h"
#import "base/values.h"
#import "ios/chrome/browser/web/web_performance_metrics/web_performance_metrics_java_script_feature_util.h"
#import "ios/chrome/browser/web/web_performance_metrics/web_performance_metrics_tab_helper.h"
#import "ios/web/public/js_messaging/java_script_feature_util.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/js_messaging/web_frame_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kPerformanceMetricsScript[] = "web_performance_metrics";
const char kWebPerformanceMetricsScriptName[] = "WebPerformanceMetricsHandler";

// The time range's expected min and max values for custom histograms.
constexpr base::TimeDelta kTimeRangeHistogramMin = base::Milliseconds(10);
constexpr base::TimeDelta kTimeRangeHistogramMax = base::Minutes(10);

// Number of buckets for the time range histograms.
constexpr int kTimeRangeHistogramBucketCount = 100;
}  // namespace

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

  if (*metric == "FirstContentfulPaint") {
    absl::optional<double> frame_navigation_start_time =
        message.body()->FindDoubleKey("frameNavigationStartTime");
    if (!frame_navigation_start_time) {
      return;
    }

    LogRelativeFirstContentfulPaint(value.value(), message.is_main_frame());
    LogAggregateFirstContentfulPaint(web_state,
                                     frame_navigation_start_time.value(),
                                     value.value(), message.is_main_frame());
  } else if (*metric == "FirstInputDelay") {
    absl::optional<bool> loaded_from_cache =
        message.body()->FindBoolKey("cached");
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
    UmaHistogramCustomTimes("IOS.Frame.FirstContentfulPaint.MainFrame",
                            base::Milliseconds(value), kTimeRangeHistogramMin,
                            kTimeRangeHistogramMax,
                            kTimeRangeHistogramBucketCount);
  } else {
    UmaHistogramCustomTimes("IOS.Frame.FirstContentfulPaint.SubFrame",
                            base::Milliseconds(value), kTimeRangeHistogramMin,
                            kTimeRangeHistogramMax,
                            kTimeRangeHistogramBucketCount);
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
        aggregate_first_contentful_paint, kTimeRangeHistogramMin,
        kTimeRangeHistogramMax, kTimeRangeHistogramBucketCount);
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

  // WebKit does not reliably support pageshow events
  // on version iOS 14 and below.
  // TODO(crbug.com/1276537)
  const bool page_show_reliably_supported =
      base::ios::IsRunningOnIOS15OrLater();

  if (is_main_frame) {
    if (!loaded_from_cache) {
      UmaHistogramCustomTimes("IOS.Frame.FirstInputDelay.MainFrame", delta,
                              kTimeRangeHistogramMin, kTimeRangeHistogramMax,
                              kTimeRangeHistogramBucketCount);
    } else if (loaded_from_cache && page_show_reliably_supported) {
      UmaHistogramCustomTimes(
          "IOS.Frame.FirstInputDelay.MainFrame.AfterBackForwardCacheRestore",
          delta, kTimeRangeHistogramMin, kTimeRangeHistogramMax,
          kTimeRangeHistogramBucketCount);
    }
  } else {
    if (!loaded_from_cache) {
      UmaHistogramCustomTimes("IOS.Frame.FirstInputDelay.SubFrame", delta,
                              kTimeRangeHistogramMin, kTimeRangeHistogramMax,
                              kTimeRangeHistogramBucketCount);
    } else if (loaded_from_cache && page_show_reliably_supported) {
      UmaHistogramCustomTimes(
          "IOS.Frame.FirstInputDelay.SubFrame.AfterBackForwardCacheRestore",
          delta, kTimeRangeHistogramMin, kTimeRangeHistogramMax,
          kTimeRangeHistogramBucketCount);
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
      UmaHistogramCustomTimes("PageLoad.InteractiveTiming.FirstInputDelay."
                              "AfterBackForwardCacheRestore",
                              delta, kTimeRangeHistogramMin,
                              kTimeRangeHistogramMax,
                              kTimeRangeHistogramBucketCount);
    } else {
      UmaHistogramCustomTimes("PageLoad.InteractiveTiming.FirstInputDelay4",
                              delta, kTimeRangeHistogramMin,
                              kTimeRangeHistogramMax,
                              kTimeRangeHistogramBucketCount);
    }
    tab_helper->SetFirstInputDelayLoggingStatus(true);
  }
}

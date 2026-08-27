// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/web_performance_metrics/web_performance_metrics_java_script_feature.h"

#import <limits>

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
constexpr char kPerformanceMetricsScript[] = "web_performance_metrics";
constexpr char kWebPerformanceMetricsScriptName[] =
    "WebPerformanceMetricsHandler";
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
  if (!message.legacy_body()->is_dict()) {
    return;
  }

  const base::DictValue& body_dict = message.legacy_body()->GetDict();

  const std::string* metric =
      body_dict.FindString(web_performance_metrics::kMetricKey);
  if (!metric || metric->empty()) {
    return;
  }

  if (*metric == web_performance_metrics::kFirstContentfulPaintMetric) {
    std::optional<double> value =
        body_dict.FindDouble(web_performance_metrics::kValueKey);
    if (!value.has_value()) {
      return;
    }
    std::optional<double> frame_navigation_start_time = body_dict.FindDouble(
        web_performance_metrics::kFrameNavigationStartTimeKey);
    if (!frame_navigation_start_time.has_value()) {
      return;
    }

    LogRelativeFirstContentfulPaint(value.value(), message.is_main_frame());
    LogAggregateFirstContentfulPaint(web_state,
                                     frame_navigation_start_time.value(),
                                     value.value(), message.is_main_frame());
  } else if (*metric == web_performance_metrics::kFirstInputDelayMetric) {
    std::optional<double> value =
        body_dict.FindDouble(web_performance_metrics::kValueKey);
    if (!value.has_value()) {
      return;
    }
    std::optional<bool> loaded_from_cache =
        body_dict.FindBool(web_performance_metrics::kCachedKey);
    if (!loaded_from_cache.has_value()) {
      return;
    }

    LogRelativeFirstInputDelay(value.value(), message.is_main_frame(),
                               loaded_from_cache.value());
    LogAggregateFirstInputDelay(web_state, value.value(),
                                loaded_from_cache.value());
  } else if (*metric ==
             web_performance_metrics::kInteractionToNextPaintMetric) {
    LogInteractionToNextPaint(web_state, body_dict, message.is_main_frame());
  }
}

void WebPerformanceMetricsJavaScriptFeature::LogInteractionToNextPaint(
    web::WebState* web_state,
    const base::DictValue& body_dict,
    bool is_main_frame) {
  // Extract durations data for the frame.
  const auto* durations_list =
      body_dict.FindList(web_performance_metrics::kDurationsKey);
  if (!durations_list) {
    return;
  }

  std::vector<base::TimeDelta> durations;
  durations.reserve(durations_list->size());
  for (const auto& val : *durations_list) {
    std::optional<double> duration = val.GetIfDouble();
    if (duration.has_value()) {
      durations.push_back(base::Milliseconds(duration.value()));
    }
  }

  // Double-check that durations did contain double values.
  if (durations.empty()) {
    return;
  }

  const std::string* frame_id =
      body_dict.FindString(web_performance_metrics::kFrameIdKey);
  int interaction_count = std::max(
      0, body_dict.FindInt(web_performance_metrics::kInteractionCountKey)
             .value_or(0));
  if (interaction_count == 0) {
    interaction_count = static_cast<int>(durations.size());
  }

  // Store the per-frame INP metrics in the TabHelper.
  if (WebPerformanceMetricsTabHelper* tab_helper =
          WebPerformanceMetricsTabHelper::FromWebState(web_state)) {
    tab_helper->SetFrameInteractionData(frame_id ? *frame_id : "", durations,
                                        interaction_count, is_main_frame);
  }
}

void WebPerformanceMetricsJavaScriptFeature::LogRelativeFirstContentfulPaint(
    double value,
    bool is_main_frame) {
  if (is_main_frame) {
    UmaHistogramCustomTimes(
        "IOS.Frame.FirstContentfulPaint.MainFrame", base::Milliseconds(value),
        web_performance_metrics::kTimeRangePaintHistogramMin,
        web_performance_metrics::kTimeRangePaintHistogramMax,
        web_performance_metrics::kTimeRangePaintHistogramBucketCount);
  } else {
    UmaHistogramCustomTimes(
        "IOS.Frame.FirstContentfulPaint.SubFrame", base::Milliseconds(value),
        web_performance_metrics::kTimeRangePaintHistogramMin,
        web_performance_metrics::kTimeRangePaintHistogramMax,
        web_performance_metrics::kTimeRangePaintHistogramBucketCount);
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
    const double main_frame_absolute =
        web_performance_metrics::CalculateAbsoluteFirstContentfulPaint(
            frame_navigation_start_time, relative_first_contentful_paint);
    web_performance_metrics::FirstContentfulPaint frame = {
        frame_navigation_start_time, relative_first_contentful_paint,
        main_frame_absolute};

    base::TimeDelta aggregate_first_contentful_paint =
        web_performance_metrics::CalculateAggregateFirstContentfulPaint(
            aggregate, frame);
    UmaHistogramCustomTimes(
        "PageLoad.PaintTiming.NavigationToFirstContentfulPaint",
        aggregate_first_contentful_paint,
        web_performance_metrics::kTimeRangePaintHistogramMin,
        web_performance_metrics::kTimeRangePaintHistogramMax,
        web_performance_metrics::kTimeRangePaintHistogramBucketCount);

    if (main_frame_absolute < aggregate) {
      tab_helper->SetAggregateAbsoluteFirstContentfulPaint(main_frame_absolute);
    }
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
      UmaHistogramCustomTimes(
          "IOS.Frame.FirstInputDelay.MainFrame2", delta,
          web_performance_metrics::kTimeRangeInteractionTimingHistogramMin,
          web_performance_metrics::kTimeRangeInteractionTimingHistogramMax,
          web_performance_metrics::
              kTimeRangeInteractionTimingHistogramBucketCount);
    } else {
      UmaHistogramCustomTimes(
          "IOS.Frame.FirstInputDelay.MainFrame.AfterBackForwardCacheRestore2",
          delta,
          web_performance_metrics::kTimeRangeInteractionTimingHistogramMin,
          web_performance_metrics::kTimeRangeInteractionTimingHistogramMax,
          web_performance_metrics::
              kTimeRangeInteractionTimingHistogramBucketCount);
    }
  } else {
    if (!loaded_from_cache) {
      UmaHistogramCustomTimes(
          "IOS.Frame.FirstInputDelay.SubFrame2", delta,
          web_performance_metrics::kTimeRangeInteractionTimingHistogramMin,
          web_performance_metrics::kTimeRangeInteractionTimingHistogramMax,
          web_performance_metrics::
              kTimeRangeInteractionTimingHistogramBucketCount);
    } else {
      UmaHistogramCustomTimes(
          "IOS.Frame.FirstInputDelay.SubFrame.AfterBackForwardCacheRestore2",
          delta,
          web_performance_metrics::kTimeRangeInteractionTimingHistogramMin,
          web_performance_metrics::kTimeRangeInteractionTimingHistogramMax,
          web_performance_metrics::
              kTimeRangeInteractionTimingHistogramBucketCount);
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
      UmaHistogramCustomTimes(
          "PageLoad.InteractiveTiming.FirstInputDelay."
          "AfterBackForwardCacheRestore_iOSFixed",
          delta,
          web_performance_metrics::kTimeRangeInteractionTimingHistogramMin,
          web_performance_metrics::kTimeRangeInteractionTimingHistogramMax,
          web_performance_metrics::
              kTimeRangeInteractionTimingHistogramBucketCount);
    } else {
      // This is an input metric for WebVitals.FirstInputDelay{2, 3} so should
      // not be deleted while those metrics still exist.
      UmaHistogramCustomTimes("PageLoad.InteractiveTiming.FirstInputDelay4",
                              delta, base::Milliseconds(10), base::Minutes(10),
                              100);
      // This is a version of the above metric that uses the same bucketing as
      // non-iOS platforms.
      UmaHistogramCustomTimes(
          "PageLoad.InteractiveTiming."
          "FirstInputDelay4_iOSFixed",
          delta,
          web_performance_metrics::kTimeRangeInteractionTimingHistogramMin,
          web_performance_metrics::kTimeRangeInteractionTimingHistogramMax,
          web_performance_metrics::
              kTimeRangeInteractionTimingHistogramBucketCount);
    }
    tab_helper->SetFirstInputDelayLoggingStatus(true);
  }
}

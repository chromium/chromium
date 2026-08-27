// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/web_performance_metrics/web_performance_metrics_tab_helper.h"

#import <algorithm>

#import "base/metrics/histogram_functions.h"

WebPerformanceMetricsTabHelper::WebPerformanceMetricsTabHelper(
    web::WebState* web_state) {
  web_state_observation_.Observe(web_state);
}

WebPerformanceMetricsTabHelper::~WebPerformanceMetricsTabHelper() = default;

void WebPerformanceMetricsTabHelper::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  FlushInteractionToNextPaintMetrics();
  SetAggregateAbsoluteFirstContentfulPaint(std::numeric_limits<double>::max());
  SetFirstInputDelayLoggingStatus(false);
  has_been_hidden_since_navigation_started_ = !web_state->IsVisible();
}

void WebPerformanceMetricsTabHelper::WasHidden(web::WebState* web_state) {
  has_been_hidden_since_navigation_started_ = true;
}

void WebPerformanceMetricsTabHelper::WebStateDestroyed(
    web::WebState* web_state) {
  FlushInteractionToNextPaintMetrics();
  web_state_observation_.Reset();
}

void WebPerformanceMetricsTabHelper::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void WebPerformanceMetricsTabHelper::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

double
WebPerformanceMetricsTabHelper::GetAggregateAbsoluteFirstContentfulPaint()
    const {
  return aggregate_absolute_first_contentful_paint_;
}

void WebPerformanceMetricsTabHelper::SetAggregateAbsoluteFirstContentfulPaint(
    double absolute_first_contentful_paint) {
  aggregate_absolute_first_contentful_paint_ = absolute_first_contentful_paint;

  // Only notify if we have a valid first contentful paint time.
  if (absolute_first_contentful_paint != std::numeric_limits<double>::max()) {
    for (auto& observer : observers_) {
      observer.OnFirstContentfulPaint(this, absolute_first_contentful_paint);
    }
  }
}

bool WebPerformanceMetricsTabHelper::GetFirstInputDelayLoggingStatus() const {
  return first_input_delay_has_been_logged_;
}

bool WebPerformanceMetricsTabHelper::HasBeenHiddenSinceNavigationStarted()
    const {
  return has_been_hidden_since_navigation_started_;
}

void WebPerformanceMetricsTabHelper::SetFirstInputDelayLoggingStatus(
    bool first_input_delay_logging_status) {
  first_input_delay_has_been_logged_ = first_input_delay_logging_status;
}

void WebPerformanceMetricsTabHelper::SetFrameInteractionData(
    const std::string& frame_id,
    const std::vector<base::TimeDelta>& longest_durations,
    int interaction_count,
    bool is_main_frame) {
  frame_interactions_[frame_id] = {
      .longest_durations = longest_durations,
      .interaction_count = interaction_count,
      .is_main_frame = is_main_frame,
  };
}

void WebPerformanceMetricsTabHelper::FlushInteractionToNextPaintMetrics() {
  if (frame_interactions_.empty()) {
    return;
  }

  // Page-wide INP metric.
  std::optional<base::TimeDelta> aggregate_inp =
      web_performance_metrics::CalculateAggregateInteractionToNextPaint(
          frame_interactions_);
  if (aggregate_inp.has_value()) {
    UmaHistogramCustomTimes(
        web_performance_metrics::kAggregateInteractionToNextPaintHistogram,
        aggregate_inp.value(),
        web_performance_metrics::kTimeRangeInteractionTimingHistogramMin,
        web_performance_metrics::kTimeRangeInteractionTimingHistogramMax,
        web_performance_metrics::
            kTimeRangeInteractionTimingHistogramBucketCount);
  }

  // Per-frame INP metric.
  for (const auto& [id, data] : frame_interactions_) {
    std::optional<base::TimeDelta> frame_inp =
        web_performance_metrics::CalculateInteractionToNextPaint(
            data.longest_durations, data.interaction_count);
    if (!frame_inp.has_value()) {
      continue;
    }

    if (data.is_main_frame) {
      UmaHistogramCustomTimes(
          web_performance_metrics::kInteractionToNextPaintMainFrameHistogram,
          frame_inp.value(),
          web_performance_metrics::kTimeRangeInteractionTimingHistogramMin,
          web_performance_metrics::kTimeRangeInteractionTimingHistogramMax,
          web_performance_metrics::
              kTimeRangeInteractionTimingHistogramBucketCount);
    } else {
      UmaHistogramCustomTimes(
          web_performance_metrics::kInteractionToNextPaintSubFrameHistogram,
          frame_inp.value(),
          web_performance_metrics::kTimeRangeInteractionTimingHistogramMin,
          web_performance_metrics::kTimeRangeInteractionTimingHistogramMax,
          web_performance_metrics::
              kTimeRangeInteractionTimingHistogramBucketCount);
    }
  }

  frame_interactions_.clear();
}

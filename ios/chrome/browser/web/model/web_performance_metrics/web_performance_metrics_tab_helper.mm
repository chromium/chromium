// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/web_performance_metrics/web_performance_metrics_tab_helper.h"

#import <algorithm>

#import "base/metrics/histogram_functions.h"
#import "ios/chrome/browser/web/model/web_performance_metrics/web_performance_metrics_java_script_feature.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/navigation/navigation_context.h"

WebPerformanceMetricsTabHelper::WebPerformanceMetricsTabHelper(
    web::WebState* web_state) {
  web_state_observation_.Observe(web_state);
  web::WebFramesManager* web_frames_manager =
      WebPerformanceMetricsJavaScriptFeature::GetInstance()
          ->GetWebFramesManager(web_state);
  web_frames_manager_observation_.Observe(web_frames_manager);
  for (web::WebFrame* web_frame : web_frames_manager->GetAllWebFrames()) {
    AddFrameToCurrentPage(web_frame->GetFrameId());
  }
}

WebPerformanceMetricsTabHelper::~WebPerformanceMetricsTabHelper() = default;

void WebPerformanceMetricsTabHelper::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  // Same-document navigations (fragment changes, `history.pushState()`) keep
  // the current document alive, so its metrics must keep accumulating and must
  // not be reset.
  if (navigation_context->IsSameDocument()) {
    return;
  }

  // TODO(crbug.com/573269319): Move these state resets to
  // `DidFinishNavigation` alongside INP.
  SetAggregateAbsoluteFirstContentfulPaint(std::numeric_limits<double>::max());
  SetFirstInputDelayLoggingStatus(false);
  has_been_hidden_since_navigation_started_ = !web_state->IsVisible();
}

void WebPerformanceMetricsTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  // Only committed cross-document navigations change the active page.
  if (!navigation_context->HasCommitted() ||
      navigation_context->IsSameDocument()) {
    return;
  }
  LogPageInteractionToNextPaint(previous_page_interactions_);

  // Shift tracking states.
  previous_page_interactions_ = std::move(current_page_interactions_);
  current_page_interactions_.clear();
  previous_frame_ids_ = std::move(current_frame_ids_);
  current_frame_ids_.clear();

  // Frames are usually registered after this point, but error pages register
  // their frame before it.
  for (web::WebFrame* web_frame :
       web_frames_manager_observation_.GetSource()->GetAllWebFrames()) {
    AddFrameToCurrentPage(web_frame->GetFrameId());
  }
}

void WebPerformanceMetricsTabHelper::WasHidden(web::WebState* web_state) {
  has_been_hidden_since_navigation_started_ = true;
}

void WebPerformanceMetricsTabHelper::WebStateDestroyed(
    web::WebState* web_state) {
  FlushInteractionToNextPaintMetrics();
  web_frames_manager_observation_.Reset();
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
  web_performance_metrics::FrameInteractionData data = {
      .longest_durations = longest_durations,
      .interaction_count = interaction_count,
      .is_main_frame = is_main_frame,
  };

  // On cross-site navigations, WebKit can deliver the previous page's last
  // message after the new page has committed.
  if (current_frame_ids_.contains(frame_id)) {
    current_page_interactions_[frame_id] = std::move(data);
  } else if (previous_frame_ids_.contains(frame_id)) {
    previous_page_interactions_[frame_id] = std::move(data);
  }
}

void WebPerformanceMetricsTabHelper::FlushInteractionToNextPaintMetrics() {
  LogPageInteractionToNextPaint(previous_page_interactions_);
  LogPageInteractionToNextPaint(current_page_interactions_);
  previous_page_interactions_.clear();
  current_page_interactions_.clear();
}

void WebPerformanceMetricsTabHelper::WebFrameBecameAvailable(
    web::WebFramesManager* web_frames_manager,
    web::WebFrame* web_frame) {
  AddFrameToCurrentPage(web_frame->GetFrameId());
}

void WebPerformanceMetricsTabHelper::AddFrameToCurrentPage(
    std::string frame_id) {
  // Clear any prior tracking (e.g. if restored from the back/forward cache).
  previous_frame_ids_.erase(frame_id);
  current_frame_ids_.insert(std::move(frame_id));
}

void WebPerformanceMetricsTabHelper::LogPageInteractionToNextPaint(
    const PageInteractions& page_interactions) {
  if (page_interactions.empty()) {
    return;
  }

  // Page-wide INP metric.
  std::optional<base::TimeDelta> aggregate_inp =
      web_performance_metrics::CalculateAggregateInteractionToNextPaint(
          page_interactions);
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
  for (const auto& [id, data] : page_interactions) {
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
}

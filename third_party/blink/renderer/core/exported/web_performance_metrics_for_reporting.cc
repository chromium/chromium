/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/public/web/web_performance_metrics_for_reporting.h"

#include "third_party/blink/renderer/core/timing/performance_timing_for_reporting.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"

namespace blink {

static double MillisecondsToSeconds(uint64_t milliseconds) {
  return static_cast<double>(milliseconds / 1000.0);
}

void WebPerformanceMetricsForReporting::Reset() {
  private_.Reset();
}

void WebPerformanceMetricsForReporting::Assign(
    const WebPerformanceMetricsForReporting& other) {
  private_ = other.private_;
}

WebNavigationType WebPerformanceMetricsForReporting::GetNavigationType() const {
  switch (private_->navigation()->type()) {
    case PerformanceNavigation::kTypeNavigate:
      return kWebNavigationTypeOther;
    case PerformanceNavigation::kTypeReload:
      return kWebNavigationTypeReload;
    case PerformanceNavigation::kTypeBackForward:
      return kWebNavigationTypeBackForward;
    case PerformanceNavigation::kTypeReserved:
      return kWebNavigationTypeOther;
  }
  NOTREACHED();
  return kWebNavigationTypeOther;
}

double WebPerformanceMetricsForReporting::NavigationStart() const {
  return MillisecondsToSeconds(private_->timing()->navigationStart());
}

base::TimeTicks
WebPerformanceMetricsForReporting::NavigationStartAsMonotonicTime() const {
  return private_->timingForReporting()->NavigationStartAsMonotonicTime();
}

WebPerformanceMetricsForReporting::BackForwardCacheRestoreTimings
WebPerformanceMetricsForReporting::BackForwardCacheRestore() const {
  PerformanceTimingForReporting::BackForwardCacheRestoreTimings
      restore_timings =
          private_->timingForReporting()->BackForwardCacheRestore();

  WebVector<BackForwardCacheRestoreTiming> timings(restore_timings.size());
  for (wtf_size_t i = 0; i < restore_timings.size(); i++) {
    timings[i].navigation_start =
        MillisecondsToSeconds(restore_timings[i].navigation_start);
    timings[i].first_paint =
        MillisecondsToSeconds(restore_timings[i].first_paint);
    for (wtf_size_t j = 0;
         j < restore_timings[i].request_animation_frames.size(); j++) {
      timings[i].request_animation_frames[j] =
          MillisecondsToSeconds(restore_timings[i].request_animation_frames[j]);
    }
    timings[i].first_input_delay = restore_timings[i].first_input_delay;
  }
  return timings;
}

double WebPerformanceMetricsForReporting::InputForNavigationStart() const {
  return MillisecondsToSeconds(private_->timingForReporting()->inputStart());
}

double WebPerformanceMetricsForReporting::ResponseStart() const {
  return MillisecondsToSeconds(private_->timing()->responseStart());
}

double WebPerformanceMetricsForReporting::DomContentLoadedEventStart() const {
  return MillisecondsToSeconds(
      private_->timing()->domContentLoadedEventStart());
}

double WebPerformanceMetricsForReporting::DomContentLoadedEventEnd() const {
  return MillisecondsToSeconds(private_->timing()->domContentLoadedEventEnd());
}

double WebPerformanceMetricsForReporting::LoadEventStart() const {
  return MillisecondsToSeconds(private_->timing()->loadEventStart());
}

double WebPerformanceMetricsForReporting::LoadEventEnd() const {
  return MillisecondsToSeconds(private_->timing()->loadEventEnd());
}

double WebPerformanceMetricsForReporting::FirstPaint() const {
  return MillisecondsToSeconds(
      private_->timingForReporting()->FirstPaintForMetrics());
}

double WebPerformanceMetricsForReporting::FirstImagePaint() const {
  return MillisecondsToSeconds(
      private_->timingForReporting()->FirstImagePaint());
}

double WebPerformanceMetricsForReporting::FirstContentfulPaint() const {
  return MillisecondsToSeconds(
      private_->timingForReporting()
          ->FirstContentfulPaintIgnoringSoftNavigations());
}

base::TimeTicks
WebPerformanceMetricsForReporting::FirstContentfulPaintAsMonotonicTime() const {
  return private_->timingForReporting()
      ->FirstContentfulPaintAsMonotonicTimeForMetrics();
}

base::TimeTicks WebPerformanceMetricsForReporting::
    FirstContentfulPaintRenderedButNotPresentedAsMonotonicTime() const {
  return private_->timingForReporting()
      ->FirstContentfulPaintRenderedButNotPresentedAsMonotonicTime();
}

double WebPerformanceMetricsForReporting::FirstMeaningfulPaint() const {
  return MillisecondsToSeconds(
      private_->timingForReporting()->FirstMeaningfulPaint());
}

double WebPerformanceMetricsForReporting::LargestImagePaintForMetrics() const {
  return MillisecondsToSeconds(
      private_->timingForReporting()->LargestImagePaintForMetrics());
}

uint64_t WebPerformanceMetricsForReporting::LargestImagePaintSizeForMetrics()
    const {
  return private_->timingForReporting()->LargestImagePaintSizeForMetrics();
}

double WebPerformanceMetricsForReporting::LargestTextPaintForMetrics() const {
  return MillisecondsToSeconds(
      private_->timingForReporting()->LargestTextPaintForMetrics());
}

uint64_t WebPerformanceMetricsForReporting::LargestTextPaintSizeForMetrics()
    const {
  return private_->timingForReporting()->LargestTextPaintSizeForMetrics();
}

base::TimeTicks WebPerformanceMetricsForReporting::
    LargestContentfulPaintAsMonotonicTimeForMetrics() const {
  return private_->timingForReporting()
      ->LargestContentfulPaintAsMonotonicTimeForMetrics();
}

double WebPerformanceMetricsForReporting::ExperimentalLargestImagePaint()
    const {
  return 0.0;
}

uint64_t WebPerformanceMetricsForReporting::ExperimentalLargestImagePaintSize()
    const {
  return 0u;
}

blink::LargestContentfulPaintType
WebPerformanceMetricsForReporting::LargestContentfulPaintTypeForMetrics()
    const {
  return private_->timingForReporting()->LargestContentfulPaintTypeForMetrics();
}

double
WebPerformanceMetricsForReporting::LargestContentfulPaintImageBPPForMetrics()
    const {
  return private_->timingForReporting()
      ->LargestContentfulPaintImageBPPForMetrics();
}

absl::optional<WebURLRequest::Priority> WebPerformanceMetricsForReporting::
    LargestContentfulPaintImageRequestPriorityForMetrics() const {
  return private_->timingForReporting()
      ->LargestContentfulPaintImageRequestPriorityForMetrics();
}

absl::optional<base::TimeDelta>
WebPerformanceMetricsForReporting::LargestContentfulPaintImageLoadStart()
    const {
  return private_->timingForReporting()->LargestContentfulPaintImageLoadStart();
}

absl::optional<base::TimeDelta>
WebPerformanceMetricsForReporting::LargestContentfulPaintImageLoadEnd() const {
  return private_->timingForReporting()->LargestContentfulPaintImageLoadEnd();
}

double WebPerformanceMetricsForReporting::ExperimentalLargestTextPaint() const {
  return 0.0;
}

uint64_t WebPerformanceMetricsForReporting::ExperimentalLargestTextPaintSize()
    const {
  return 0u;
}

double WebPerformanceMetricsForReporting::FirstEligibleToPaint() const {
  return MillisecondsToSeconds(
      private_->timingForReporting()->FirstEligibleToPaint());
}

double WebPerformanceMetricsForReporting::FirstInputOrScrollNotifiedTimestamp()
    const {
  return MillisecondsToSeconds(
      private_->timingForReporting()->FirstInputOrScrollNotifiedTimestamp());
}

absl::optional<base::TimeDelta>
WebPerformanceMetricsForReporting::FirstInputDelay() const {
  return private_->timingForReporting()->FirstInputDelay();
}

absl::optional<base::TimeDelta>
WebPerformanceMetricsForReporting::FirstInputTimestamp() const {
  return private_->timingForReporting()->FirstInputTimestamp();
}

absl::optional<base::TimeTicks>
WebPerformanceMetricsForReporting::FirstInputTimestampAsMonotonicTime() const {
  return private_->timingForReporting()->FirstInputTimestampAsMonotonicTime();
}

absl::optional<base::TimeDelta>
WebPerformanceMetricsForReporting::LongestInputDelay() const {
  return private_->timingForReporting()->LongestInputDelay();
}

absl::optional<base::TimeDelta>
WebPerformanceMetricsForReporting::LongestInputTimestamp() const {
  return private_->timingForReporting()->LongestInputTimestamp();
}

absl::optional<base::TimeDelta>
WebPerformanceMetricsForReporting::FirstInputProcessingTime() const {
  return private_->timingForReporting()->FirstInputProcessingTime();
}

absl::optional<base::TimeDelta>
WebPerformanceMetricsForReporting::FirstScrollDelay() const {
  return private_->timingForReporting()->FirstScrollDelay();
}

absl::optional<base::TimeDelta>
WebPerformanceMetricsForReporting::FirstScrollTimestamp() const {
  return private_->timingForReporting()->FirstScrollTimestamp();
}

double WebPerformanceMetricsForReporting::ParseStart() const {
  return MillisecondsToSeconds(private_->timingForReporting()->ParseStart());
}

double WebPerformanceMetricsForReporting::ParseStop() const {
  return MillisecondsToSeconds(private_->timingForReporting()->ParseStop());
}

double WebPerformanceMetricsForReporting::ParseBlockedOnScriptLoadDuration()
    const {
  return MillisecondsToSeconds(
      private_->timingForReporting()->ParseBlockedOnScriptLoadDuration());
}

double WebPerformanceMetricsForReporting::
    ParseBlockedOnScriptLoadFromDocumentWriteDuration() const {
  return MillisecondsToSeconds(
      private_->timingForReporting()
          ->ParseBlockedOnScriptLoadFromDocumentWriteDuration());
}

double
WebPerformanceMetricsForReporting::ParseBlockedOnScriptExecutionDuration()
    const {
  return MillisecondsToSeconds(
      private_->timingForReporting()->ParseBlockedOnScriptExecutionDuration());
}

double WebPerformanceMetricsForReporting::
    ParseBlockedOnScriptExecutionFromDocumentWriteDuration() const {
  return MillisecondsToSeconds(
      private_->timingForReporting()
          ->ParseBlockedOnScriptExecutionFromDocumentWriteDuration());
}

absl::optional<base::TimeTicks>
WebPerformanceMetricsForReporting::LastPortalActivatedPaint() const {
  return private_->timingForReporting()->LastPortalActivatedPaint();
}

absl::optional<base::TimeDelta>
WebPerformanceMetricsForReporting::PrerenderActivationStart() const {
  return private_->timingForReporting()->PrerenderActivationStart();
}

absl::optional<base::TimeDelta>
WebPerformanceMetricsForReporting::UserTimingMarkFullyLoaded() const {
  return private_->timingForReporting()->UserTimingMarkFullyLoaded();
}

absl::optional<base::TimeDelta>
WebPerformanceMetricsForReporting::UserTimingMarkFullyVisible() const {
  return private_->timingForReporting()->UserTimingMarkFullyVisible();
}

absl::optional<base::TimeDelta>
WebPerformanceMetricsForReporting::UserTimingMarkInteractive() const {
  return private_->timingForReporting()->UserTimingMarkInteractive();
}

WebPerformanceMetricsForReporting::WebPerformanceMetricsForReporting(
    WindowPerformance* performance)
    : private_(performance) {}

WebPerformanceMetricsForReporting& WebPerformanceMetricsForReporting::operator=(
    WindowPerformance* performance) {
  private_ = performance;
  return *this;
}

}  // namespace blink

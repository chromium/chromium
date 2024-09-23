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

#include "third_party/blink/public/common/performance/largest_contentful_paint_type.h"
#include "third_party/blink/renderer/core/timing/performance_timing_for_reporting.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"

namespace blink {

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
  NOTREACHED_IN_MIGRATION();
  return kWebNavigationTypeOther;
}

double WebPerformanceMetricsForReporting::NavigationStart() const {
  return base::Milliseconds(private_->timing()->navigationStart()).InSecondsF();
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
        base::Milliseconds(restore_timings[i].navigation_start).InSecondsF();
    timings[i].first_paint =
        base::Milliseconds(restore_timings[i].first_paint).InSecondsF();
    for (wtf_size_t j = 0;
         j < restore_timings[i].request_animation_frames.size(); j++) {
      timings[i].request_animation_frames[j] =
          base::Milliseconds(restore_timings[i].request_animation_frames[j])
              .InSecondsF();
    }
    timings[i].first_input_delay = restore_timings[i].first_input_delay;
  }
  return timings;
}

double WebPerformanceMetricsForReporting::InputForNavigationStart() const {
  return base::Milliseconds(private_->timingForReporting()->inputStart())
      .InSecondsF();
}

double WebPerformanceMetricsForReporting::ResponseStart() const {
  return base::Milliseconds(private_->timing()->responseStart()).InSecondsF();
}

double WebPerformanceMetricsForReporting::DomainLookupStart() const {
  return base::Milliseconds(private_->timing()->domainLookupStart())
      .InSecondsF();
}

double WebPerformanceMetricsForReporting::DomainLookupEnd() const {
  return base::Milliseconds(private_->timing()->domainLookupEnd()).InSecondsF();
}

double WebPerformanceMetricsForReporting::ConnectStart() const {
  return base::Milliseconds(private_->timing()->connectStart()).InSecondsF();
}

double WebPerformanceMetricsForReporting::DomContentLoadedEventStart() const {
  return base::Milliseconds(private_->timing()->domContentLoadedEventStart())
      .InSecondsF();
}

double WebPerformanceMetricsForReporting::DomContentLoadedEventEnd() const {
  return base::Milliseconds(private_->timing()->domContentLoadedEventEnd())
      .InSecondsF();
}

double WebPerformanceMetricsForReporting::LoadEventStart() const {
  return base::Milliseconds(private_->timing()->loadEventStart()).InSecondsF();
}

double WebPerformanceMetricsForReporting::LoadEventEnd() const {
  return base::Milliseconds(private_->timing()->loadEventEnd()).InSecondsF();
}

double WebPerformanceMetricsForReporting::FirstPaint() const {
  return base::Milliseconds(
             private_->timingForReporting()->FirstPaintForMetrics())
      .InSecondsF();
}

double WebPerformanceMetricsForReporting::FirstImagePaint() const {
  return base::Milliseconds(private_->timingForReporting()->FirstImagePaint())
      .InSecondsF();
}

double WebPerformanceMetricsForReporting::FirstContentfulPaint() const {
  return base::Milliseconds(private_->timingForReporting()
                                ->FirstContentfulPaintIgnoringSoftNavigations())
      .InSecondsF();
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
  return base::Milliseconds(
             private_->timingForReporting()->FirstMeaningfulPaint())
      .InSecondsF();
}

LargestContentfulPaintDetailsForReporting
WebPerformanceMetricsForReporting::LargestContentfulDetailsForMetrics() const {
  return (private_->timingForReporting()
              ->LargestContentfulPaintDetailsForMetrics());
}

LargestContentfulPaintDetailsForReporting WebPerformanceMetricsForReporting::
    SoftNavigationLargestContentfulDetailsForMetrics() const {
  return (private_->timingForReporting()
              ->SoftNavigationLargestContentfulPaintDetailsForMetrics());
}

double WebPerformanceMetricsForReporting::FirstEligibleToPaint() const {
  return base::Milliseconds(
             private_->timingForReporting()->FirstEligibleToPaint())
      .InSecondsF();
}

double WebPerformanceMetricsForReporting::FirstInputOrScrollNotifiedTimestamp()
    const {
  return base::Milliseconds(private_->timingForReporting()
                                ->FirstInputOrScrollNotifiedTimestamp())
      .InSecondsF();
}

std::optional<base::TimeDelta>
WebPerformanceMetricsForReporting::FirstInputDelay() const {
  return private_->timingForReporting()->FirstInputDelay();
}

std::optional<base::TimeDelta>
WebPerformanceMetricsForReporting::FirstInputTimestamp() const {
  return private_->timingForReporting()->FirstInputTimestamp();
}

std::optional<base::TimeTicks>
WebPerformanceMetricsForReporting::FirstInputTimestampAsMonotonicTime() const {
  return private_->timingForReporting()->FirstInputTimestampAsMonotonicTime();
}

std::optional<base::TimeDelta>
WebPerformanceMetricsForReporting::FirstScrollDelay() const {
  return private_->timingForReporting()->FirstScrollDelay();
}

std::optional<base::TimeDelta>
WebPerformanceMetricsForReporting::FirstScrollTimestamp() const {
  return private_->timingForReporting()->FirstScrollTimestamp();
}

double WebPerformanceMetricsForReporting::ParseStart() const {
  return base::Milliseconds(private_->timingForReporting()->ParseStart())
      .InSecondsF();
}

double WebPerformanceMetricsForReporting::ParseStop() const {
  return base::Milliseconds(private_->timingForReporting()->ParseStop())
      .InSecondsF();
}

double WebPerformanceMetricsForReporting::ParseBlockedOnScriptLoadDuration()
    const {
  return base::Milliseconds(
             private_->timingForReporting()->ParseBlockedOnScriptLoadDuration())
      .InSecondsF();
}

double WebPerformanceMetricsForReporting::
    ParseBlockedOnScriptLoadFromDocumentWriteDuration() const {
  return base::Milliseconds(
             private_->timingForReporting()
                 ->ParseBlockedOnScriptLoadFromDocumentWriteDuration())
      .InSecondsF();
}

double
WebPerformanceMetricsForReporting::ParseBlockedOnScriptExecutionDuration()
    const {
  return base::Milliseconds(private_->timingForReporting()
                                ->ParseBlockedOnScriptExecutionDuration())
      .InSecondsF();
}

double WebPerformanceMetricsForReporting::
    ParseBlockedOnScriptExecutionFromDocumentWriteDuration() const {
  return base::Milliseconds(
             private_->timingForReporting()
                 ->ParseBlockedOnScriptExecutionFromDocumentWriteDuration())
      .InSecondsF();
}

std::optional<base::TimeDelta>
WebPerformanceMetricsForReporting::PrerenderActivationStart() const {
  return private_->timingForReporting()->PrerenderActivationStart();
}

std::optional<base::TimeDelta>
WebPerformanceMetricsForReporting::UserTimingMarkFullyLoaded() const {
  return private_->timingForReporting()->UserTimingMarkFullyLoaded();
}

std::optional<base::TimeDelta>
WebPerformanceMetricsForReporting::UserTimingMarkFullyVisible() const {
  return private_->timingForReporting()->UserTimingMarkFullyVisible();
}

std::optional<base::TimeDelta>
WebPerformanceMetricsForReporting::UserTimingMarkInteractive() const {
  return private_->timingForReporting()->UserTimingMarkInteractive();
}

std::optional<std::tuple<std::string, base::TimeDelta>>
WebPerformanceMetricsForReporting::CustomUserTimingMark() const {
  auto mark = private_->timingForReporting()->CustomUserTimingMark();
  if (!mark) {
    return std::nullopt;
  }
  const auto [name, start_time] = mark.value();

  return std::make_tuple(name.Utf8(), start_time);
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

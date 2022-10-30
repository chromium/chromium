// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_performance.h"

#include "third_party/blink/renderer/core/timing/performance_timing_for_reporting.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"

namespace blink {

static double MillisecondsToSeconds(uint64_t milliseconds) {
  return static_cast<double>(milliseconds / 1000.0);
}

void WebPerformance::Reset() {
  private_.Reset();
}

void WebPerformance::Assign(const WebPerformance& other) {
  private_ = other.private_;
}

WebNavigationType WebPerformance::GetNavigationType() const {
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

double WebPerformance::NavigationStart() const {
  return MillisecondsToSeconds(private_->timing()->navigationStart());
}

base::TimeTicks WebPerformance::NavigationStartAsMonotonicTime() const {
  return private_->timingForReporting()->NavigationStartAsMonotonicTime();
}

WebPerformance::BackForwardCacheRestoreTimings
WebPerformance::BackForwardCacheRestore() const {
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

double WebPerformance::InputForNavigationStart() const {
  return MillisecondsToSeconds(private_->timingForReporting()->inputStart());
}

double WebPerformance::UnloadEventEnd() const {
  return MillisecondsToSeconds(private_->timing()->unloadEventEnd());
}

double WebPerformance::RedirectStart() const {
  return MillisecondsToSeconds(private_->timing()->redirectStart());
}

double WebPerformance::RedirectEnd() const {
  return MillisecondsToSeconds(private_->timing()->redirectEnd());
}

uint16_t WebPerformance::RedirectCount() const {
  return private_->navigation()->redirectCount();
}

double WebPerformance::FetchStart() const {
  return MillisecondsToSeconds(private_->timing()->fetchStart());
}

double WebPerformance::DomainLookupStart() const {
  return MillisecondsToSeconds(private_->timing()->domainLookupStart());
}

double WebPerformance::DomainLookupEnd() const {
  return MillisecondsToSeconds(private_->timing()->domainLookupEnd());
}

double WebPerformance::ConnectStart() const {
  return MillisecondsToSeconds(private_->timing()->connectStart());
}

double WebPerformance::ConnectEnd() const {
  return MillisecondsToSeconds(private_->timing()->connectEnd());
}

double WebPerformance::RequestStart() const {
  return MillisecondsToSeconds(private_->timing()->requestStart());
}

double WebPerformance::ResponseStart() const {
  return MillisecondsToSeconds(private_->timing()->responseStart());
}

double WebPerformance::ResponseEnd() const {
  return MillisecondsToSeconds(private_->timing()->responseEnd());
}

double WebPerformance::DomLoading() const {
  return MillisecondsToSeconds(private_->timing()->domLoading());
}

double WebPerformance::DomInteractive() const {
  return MillisecondsToSeconds(private_->timing()->domInteractive());
}

double WebPerformance::DomContentLoadedEventStart() const {
  return MillisecondsToSeconds(
      private_->timing()->domContentLoadedEventStart());
}

double WebPerformance::DomContentLoadedEventEnd() const {
  return MillisecondsToSeconds(private_->timing()->domContentLoadedEventEnd());
}

double WebPerformance::DomComplete() const {
  return MillisecondsToSeconds(private_->timing()->domComplete());
}

double WebPerformance::LoadEventStart() const {
  return MillisecondsToSeconds(private_->timing()->loadEventStart());
}

double WebPerformance::LoadEventEnd() const {
  return MillisecondsToSeconds(private_->timing()->loadEventEnd());
}

double WebPerformance::FirstPaint() const {
  return MillisecondsToSeconds(
      private_->timingForReporting()->FirstPaintForMetrics());
}

double WebPerformance::FirstImagePaint() const {
  return MillisecondsToSeconds(
      private_->timingForReporting()->FirstImagePaint());
}

double WebPerformance::FirstContentfulPaint() const {
  return MillisecondsToSeconds(
      private_->timingForReporting()
          ->FirstContentfulPaintIgnoringSoftNavigations());
}

base::TimeTicks WebPerformance::FirstContentfulPaintAsMonotonicTime() const {
  return private_->timingForReporting()
      ->FirstContentfulPaintAsMonotonicTimeForMetrics();
}

base::TimeTicks
WebPerformance::FirstContentfulPaintRenderedButNotPresentedAsMonotonicTime()
    const {
  return private_->timingForReporting()
      ->FirstContentfulPaintRenderedButNotPresentedAsMonotonicTime();
}

double WebPerformance::FirstMeaningfulPaint() const {
  return MillisecondsToSeconds(
      private_->timingForReporting()->FirstMeaningfulPaint());
}

double WebPerformance::FirstMeaningfulPaintCandidate() const {
  return MillisecondsToSeconds(
      private_->timingForReporting()->FirstMeaningfulPaintCandidate());
}

double WebPerformance::LargestImagePaintForMetrics() const {
  return MillisecondsToSeconds(
      private_->timingForReporting()->LargestImagePaintForMetrics());
}

uint64_t WebPerformance::LargestImagePaintSizeForMetrics() const {
  return private_->timingForReporting()->LargestImagePaintSizeForMetrics();
}

double WebPerformance::LargestTextPaintForMetrics() const {
  return MillisecondsToSeconds(
      private_->timingForReporting()->LargestTextPaintForMetrics());
}

uint64_t WebPerformance::LargestTextPaintSizeForMetrics() const {
  return private_->timingForReporting()->LargestTextPaintSizeForMetrics();
}

base::TimeTicks
WebPerformance::LargestContentfulPaintAsMonotonicTimeForMetrics() const {
  return private_->timingForReporting()
      ->LargestContentfulPaintAsMonotonicTimeForMetrics();
}

double WebPerformance::ExperimentalLargestImagePaint() const {
  return 0.0;
}

uint64_t WebPerformance::ExperimentalLargestImagePaintSize() const {
  return 0u;
}

blink::LargestContentfulPaintType
WebPerformance::LargestContentfulPaintTypeForMetrics() const {
  return private_->timingForReporting()->LargestContentfulPaintTypeForMetrics();
}

double WebPerformance::LargestContentfulPaintImageBPPForMetrics() const {
  return private_->timingForReporting()
      ->LargestContentfulPaintImageBPPForMetrics();
}

absl::optional<WebURLRequest::Priority>
WebPerformance::LargestContentfulPaintImageRequestPriorityForMetrics() const {
  return private_->timingForReporting()
      ->LargestContentfulPaintImageRequestPriorityForMetrics();
}

double WebPerformance::ExperimentalLargestTextPaint() const {
  return 0.0;
}

uint64_t WebPerformance::ExperimentalLargestTextPaintSize() const {
  return 0u;
}

double WebPerformance::FirstEligibleToPaint() const {
  return MillisecondsToSeconds(
      private_->timingForReporting()->FirstEligibleToPaint());
}

double WebPerformance::FirstInputOrScrollNotifiedTimestamp() const {
  return MillisecondsToSeconds(
      private_->timingForReporting()->FirstInputOrScrollNotifiedTimestamp());
}

absl::optional<base::TimeDelta> WebPerformance::FirstInputDelay() const {
  return private_->timingForReporting()->FirstInputDelay();
}

absl::optional<base::TimeDelta> WebPerformance::FirstInputTimestamp() const {
  return private_->timingForReporting()->FirstInputTimestamp();
}

absl::optional<base::TimeTicks>
WebPerformance::FirstInputTimestampAsMonotonicTime() const {
  return private_->timingForReporting()->FirstInputTimestampAsMonotonicTime();
}

absl::optional<base::TimeDelta> WebPerformance::LongestInputDelay() const {
  return private_->timingForReporting()->LongestInputDelay();
}

absl::optional<base::TimeDelta> WebPerformance::LongestInputTimestamp() const {
  return private_->timingForReporting()->LongestInputTimestamp();
}

absl::optional<base::TimeDelta> WebPerformance::FirstInputProcessingTime()
    const {
  return private_->timingForReporting()->FirstInputProcessingTime();
}

absl::optional<base::TimeDelta> WebPerformance::FirstScrollDelay() const {
  return private_->timingForReporting()->FirstScrollDelay();
}

absl::optional<base::TimeDelta> WebPerformance::FirstScrollTimestamp() const {
  return private_->timingForReporting()->FirstScrollTimestamp();
}

double WebPerformance::ParseStart() const {
  return MillisecondsToSeconds(private_->timingForReporting()->ParseStart());
}

double WebPerformance::ParseStop() const {
  return MillisecondsToSeconds(private_->timingForReporting()->ParseStop());
}

double WebPerformance::ParseBlockedOnScriptLoadDuration() const {
  return MillisecondsToSeconds(
      private_->timingForReporting()->ParseBlockedOnScriptLoadDuration());
}

double WebPerformance::ParseBlockedOnScriptLoadFromDocumentWriteDuration()
    const {
  return MillisecondsToSeconds(
      private_->timingForReporting()
          ->ParseBlockedOnScriptLoadFromDocumentWriteDuration());
}

double WebPerformance::ParseBlockedOnScriptExecutionDuration() const {
  return MillisecondsToSeconds(
      private_->timingForReporting()->ParseBlockedOnScriptExecutionDuration());
}

double WebPerformance::ParseBlockedOnScriptExecutionFromDocumentWriteDuration()
    const {
  return MillisecondsToSeconds(
      private_->timingForReporting()
          ->ParseBlockedOnScriptExecutionFromDocumentWriteDuration());
}

absl::optional<base::TimeTicks> WebPerformance::LastPortalActivatedPaint()
    const {
  return private_->timingForReporting()->LastPortalActivatedPaint();
}

absl::optional<base::TimeDelta> WebPerformance::PrerenderActivationStart()
    const {
  return private_->timingForReporting()->PrerenderActivationStart();
}

absl::optional<base::TimeTicks> WebPerformance::UnloadStart() const {
  return private_->timingForReporting()->UnloadStart();
}

absl::optional<base::TimeTicks> WebPerformance::UnloadEnd() const {
  return private_->timingForReporting()->UnloadEnd();
}

absl::optional<base::TimeTicks> WebPerformance::CommitNavigationEnd() const {
  return private_->timingForReporting()->CommitNavigationEnd();
}

absl::optional<base::TimeDelta> WebPerformance::UserTimingMarkFullyLoaded()
    const {
  return private_->timingForReporting()->UserTimingMarkFullyLoaded();
}

absl::optional<base::TimeDelta> WebPerformance::UserTimingMarkFullyVisible()
    const {
  return private_->timingForReporting()->UserTimingMarkFullyVisible();
}

absl::optional<base::TimeDelta> WebPerformance::UserTimingMarkInteractive()
    const {
  return private_->timingForReporting()->UserTimingMarkInteractive();
}

WebPerformance::WebPerformance(WindowPerformance* performance)
    : private_(performance) {}

WebPerformance& WebPerformance::operator=(WindowPerformance* performance) {
  private_ = performance;
  return *this;
}

}  // namespace blink

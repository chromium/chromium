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

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_PERFORMANCE_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_PERFORMANCE_H_

#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/performance/largest_contentful_paint_type.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_private_ptr.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_navigation_type.h"

namespace blink {

class WindowPerformance;

class BLINK_EXPORT WebPerformance {
 public:
  // The count to record the times on requestAnimationFrame after the page is
  // restored from the back-forward cache.
  static constexpr int
      kRequestAnimationFramesToRecordAfterBackForwardCacheRestore = 3;

  struct BackForwardCacheRestoreTiming {
    double navigation_start = 0;
    double first_paint = 0;
    std::array<double,
               kRequestAnimationFramesToRecordAfterBackForwardCacheRestore>
        request_animation_frames = {};
    absl::optional<base::TimeDelta> first_input_delay;
  };

  using BackForwardCacheRestoreTimings =
      WebVector<BackForwardCacheRestoreTiming>;

  ~WebPerformance() { Reset(); }

  WebPerformance() = default;

  WebPerformance(const WebPerformance& p) { Assign(p); }

  WebPerformance& operator=(const WebPerformance& p) {
    Assign(p);
    return *this;
  }

  void Reset();
  void Assign(const WebPerformance&);

  // This only returns one of {Other|Reload|BackForward}.
  // Form submits and link clicks all fall under other.
  WebNavigationType GetNavigationType() const;

  // These functions return time in seconds (not milliseconds) since the epoch.
  double InputForNavigationStart() const;
  double NavigationStart() const;
  base::TimeTicks NavigationStartAsMonotonicTime() const;
  BackForwardCacheRestoreTimings BackForwardCacheRestore() const;
  double UnloadEventEnd() const;
  double RedirectStart() const;
  double RedirectEnd() const;
  uint16_t RedirectCount() const;
  double FetchStart() const;
  double DomainLookupStart() const;
  double DomainLookupEnd() const;
  double ConnectStart() const;
  double ConnectEnd() const;
  double RequestStart() const;
  double ResponseStart() const;
  double ResponseEnd() const;
  double DomLoading() const;
  double DomInteractive() const;
  double DomContentLoadedEventStart() const;
  double DomContentLoadedEventEnd() const;
  double DomComplete() const;
  double LoadEventStart() const;
  double LoadEventEnd() const;
  double FirstPaint() const;
  double FirstImagePaint() const;
  double FirstContentfulPaint() const;
  base::TimeTicks FirstContentfulPaintAsMonotonicTime() const;
  base::TimeTicks FirstContentfulPaintRenderedButNotPresentedAsMonotonicTime()
      const;
  double FirstMeaningfulPaint() const;
  double FirstMeaningfulPaintCandidate() const;
  double LargestImagePaintForMetrics() const;
  uint64_t LargestImagePaintSizeForMetrics() const;
  double LargestTextPaintForMetrics() const;
  uint64_t LargestTextPaintSizeForMetrics() const;
  base::TimeTicks LargestContentfulPaintAsMonotonicTimeForMetrics() const;
  double ExperimentalLargestImagePaint() const;
  uint64_t ExperimentalLargestImagePaintSize() const;
  blink::LargestContentfulPaintType LargestContentfulPaintTypeForMetrics()
      const;
  double LargestContentfulPaintImageBPPForMetrics() const;
  double ExperimentalLargestTextPaint() const;
  uint64_t ExperimentalLargestTextPaintSize() const;
  double FirstEligibleToPaint() const;
  double FirstInputOrScrollNotifiedTimestamp() const;
  absl::optional<base::TimeDelta> FirstInputDelay() const;
  absl::optional<base::TimeDelta> FirstInputTimestamp() const;
  absl::optional<base::TimeTicks> FirstInputTimestampAsMonotonicTime() const;
  absl::optional<base::TimeDelta> LongestInputDelay() const;
  absl::optional<base::TimeDelta> LongestInputTimestamp() const;
  absl::optional<base::TimeDelta> FirstInputProcessingTime() const;
  absl::optional<base::TimeDelta> FirstScrollDelay() const;
  absl::optional<base::TimeDelta> FirstScrollTimestamp() const;
  double ParseStart() const;
  double ParseStop() const;
  double ParseBlockedOnScriptLoadDuration() const;
  double ParseBlockedOnScriptLoadFromDocumentWriteDuration() const;
  double ParseBlockedOnScriptExecutionDuration() const;
  double ParseBlockedOnScriptExecutionFromDocumentWriteDuration() const;
  absl::optional<base::TimeTicks> LastPortalActivatedPaint() const;
  absl::optional<base::TimeDelta> PrerenderActivationStart() const;
  absl::optional<base::TimeTicks> UnloadStart() const;
  absl::optional<base::TimeTicks> UnloadEnd() const;
  absl::optional<base::TimeTicks> CommitNavigationEnd() const;
  absl::optional<base::TimeDelta> UserTimingMarkFullyLoaded() const;
  absl::optional<base::TimeDelta> UserTimingMarkFullyVisible() const;
  absl::optional<base::TimeDelta> UserTimingMarkInteractive() const;

#if INSIDE_BLINK
  WebPerformance(WindowPerformance*);
  WebPerformance& operator=(WindowPerformance*);
#endif

 private:
  WebPrivatePtr<WindowPerformance> private_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_PERFORMANCE_H_

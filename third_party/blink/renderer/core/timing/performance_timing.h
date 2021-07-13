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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_TIMING_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_TIMING_H_

#include "base/time/time.h"
#include "third_party/blink/public/web/web_performance.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"

namespace blink {

class CSSTiming;
class DocumentLoadTiming;
class DocumentLoader;
class DocumentParserTiming;
class DocumentTiming;
class InteractiveDetector;
class PaintTiming;
class PaintTimingDetector;
class ResourceLoadTiming;
class ScriptState;
class ScriptValue;

// Legacy support for NT1(https://www.w3.org/TR/navigation-timing/).
class CORE_EXPORT PerformanceTiming final : public ScriptWrappable,
                                            public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  struct BackForwardCacheRestoreTiming {
    uint64_t navigation_start;
    uint64_t first_paint;
    std::array<uint64_t,
               WebPerformance::
                   kRequestAnimationFramesToRecordAfterBackForwardCacheRestore>
        request_animation_frames;
    absl::optional<base::TimeDelta> first_input_delay;
  };

  using BackForwardCacheRestoreTimings =
      WTF::Vector<BackForwardCacheRestoreTiming>;

  explicit PerformanceTiming(ExecutionContext*);

  uint64_t navigationStart() const;
  uint64_t inputStart() const;
  uint64_t unloadEventStart() const;
  uint64_t unloadEventEnd() const;
  uint64_t redirectStart() const;
  uint64_t redirectEnd() const;
  uint64_t fetchStart() const;
  uint64_t domainLookupStart() const;
  uint64_t domainLookupEnd() const;
  uint64_t connectStart() const;
  uint64_t connectEnd() const;
  uint64_t secureConnectionStart() const;
  uint64_t requestStart() const;
  uint64_t responseStart() const;
  uint64_t responseEnd() const;
  uint64_t domLoading() const;
  uint64_t domInteractive() const;
  uint64_t domContentLoadedEventStart() const;
  uint64_t domContentLoadedEventEnd() const;
  uint64_t domComplete() const;
  uint64_t loadEventStart() const;
  uint64_t loadEventEnd() const;

  // The below are non-spec timings, for Page Load UMA metrics. Not to be
  // exposed to JavaScript.

  // The time immediately after the user agent finishes prompting to unload the
  // previous document, or if there is no previous document, the same value as
  // fetchStart.  Intended to be used for correlation with other events internal
  // to blink.
  base::TimeTicks NavigationStartAsMonotonicTime() const;
  // The timings after the page is restored from back-forward cache.
  BackForwardCacheRestoreTimings BackForwardCacheRestore() const;
  // The time the first paint operation was performed.
  uint64_t FirstPaint() const;
  // The time the first paint operation for image was performed.
  uint64_t FirstImagePaint() const;
  // The time of the first 'contentful' paint. A contentful paint is a paint
  // that includes content of some kind (for example, text or image content).
  uint64_t FirstContentfulPaint() const;
  // The first 'contentful' paint as full-resolution monotonic time. Intended to
  // be used for correlation with other events internal to blink.
  base::TimeTicks FirstContentfulPaintAsMonotonicTime() const;
  // The time of the first 'meaningful' paint, A meaningful paint is a paint
  // where the page's primary content is visible.
  uint64_t FirstMeaningfulPaint() const;
  // The time of the candidate of first 'meaningful' paint, A meaningful paint
  // candidate indicates the first time we considered a paint to qualify as the
  // potential first meaningful paint. But, be careful that it may be an
  // optimistic (i.e., too early) estimate.
  // TODO(crbug.com/848639): This function is exposed as an experiment, and if
  // not useful, this function can be removed.
  uint64_t FirstMeaningfulPaintCandidate() const;
  // Largest Image Paint is the first paint after the largest image within
  // viewport being fully loaded. LargestImagePaint and LargestImagePaintSize
  // are the time and size of it.
  uint64_t LargestImagePaint() const;
  uint64_t LargestImagePaintSize() const;
  // The time of the first paint of the largest text within viewport.
  // Largest Text Paint is the first paint after the largest text within
  // viewport being painted. LargestTextPaint and LargestTextPaintSize
  // are the time and size of it.
  uint64_t LargestTextPaint() const;
  uint64_t LargestTextPaintSize() const;
  // Largest Contentful Paint is the either the largest text paint time or the
  // largest image paint time, whichever has the larger size.
  base::TimeTicks LargestContentfulPaintAsMonotonicTime() const;
  // Experimental versions of the above metrics. Currently these are computed by
  // considering the largest content seen so far, regardless of DOM node
  // removal.
  uint64_t ExperimentalLargestImagePaint() const;
  uint64_t ExperimentalLargestImagePaintSize() const;
  uint64_t ExperimentalLargestTextPaint() const;
  uint64_t ExperimentalLargestTextPaintSize() const;
  // The time at which the frame is first eligible for painting due to not
  // being throttled. A zero value indicates throttling.
  uint64_t FirstEligibleToPaint() const;
  // The time at which we are notified of the first input or scroll event which
  // causes the largest contentful paint algorithm to stop.
  uint64_t FirstInputOrScrollNotifiedTimestamp() const;
  // The duration between the hardware timestamp and being queued on the main
  // thread for the first click, tap, key press, cancellable touchstart, or
  // pointer down followed by a pointer up.
  absl::optional<base::TimeDelta> FirstInputDelay() const;
  // The timestamp of the event whose delay is reported by FirstInputDelay().
  absl::optional<base::TimeDelta> FirstInputTimestamp() const;
  // The longest duration between the hardware timestamp and being queued on the
  // main thread for the click, tap, key press, cancellable touchstart, or
  // pointer down followed by a pointer up.
  absl::optional<base::TimeDelta> LongestInputDelay() const;
  // The timestamp of the event whose delay is reported by LongestInputDelay().
  absl::optional<base::TimeDelta> LongestInputTimestamp() const;
  // The duration of event handlers processing the first input event.
  absl::optional<base::TimeDelta> FirstInputProcessingTime() const;
  // The duration between the user's first scroll and display update.
  absl::optional<base::TimeDelta> FirstScrollDelay() const;
  // The hardware timestamp of the first scroll.
  absl::optional<base::TimeDelta> FirstScrollTimestamp() const;
  // TimeTicks for unload start and end.
  absl::optional<base::TimeTicks> UnloadStart() const;
  absl::optional<base::TimeTicks> UnloadEnd() const;
  // The timestamp of when the commit navigation finished in the frame loader.
  absl::optional<base::TimeTicks> CommitNavigationEnd() const;
  // The timestamp of the user timing mark 'mark_fully_loaded', if
  // available.
  absl::optional<base::TimeDelta> UserTimingMarkFullyLoaded() const;
  // The timestamp of the user timing mark 'mark_fully_visible', if
  // available.
  absl::optional<base::TimeDelta> UserTimingMarkFullyVisible() const;
  // The timestamp of the user timing mark 'mark_interactive', if
  // available.
  absl::optional<base::TimeDelta> UserTimingMarkInteractive() const;

  uint64_t ParseStart() const;
  uint64_t ParseStop() const;
  uint64_t ParseBlockedOnScriptLoadDuration() const;
  uint64_t ParseBlockedOnScriptLoadFromDocumentWriteDuration() const;
  uint64_t ParseBlockedOnScriptExecutionDuration() const;
  uint64_t ParseBlockedOnScriptExecutionFromDocumentWriteDuration() const;

  // The time of the first paint after a portal activation.
  absl::optional<base::TimeTicks> LastPortalActivatedPaint() const;
  // The start time of the prerender activation navigation.
  absl::optional<base::TimeDelta> PrerenderActivationStart() const;

  typedef uint64_t (PerformanceTiming::*PerformanceTimingGetter)() const;
  using NameToAttributeMap = HashMap<AtomicString, PerformanceTimingGetter>;
  static const NameToAttributeMap& GetAttributeMapping();

  ScriptValue toJSONForBinding(ScriptState*) const;

  void Trace(Visitor*) const override;

  uint64_t MonotonicTimeToIntegerMilliseconds(base::TimeTicks) const;

  std::unique_ptr<TracedValue> GetNavigationTracingData();

 private:
  const DocumentTiming* GetDocumentTiming() const;
  const CSSTiming* CssTiming() const;
  const DocumentParserTiming* GetDocumentParserTiming() const;
  const PaintTiming* GetPaintTiming() const;
  PaintTimingDetector* GetPaintTimingDetector() const;
  DocumentLoader* GetDocumentLoader() const;
  DocumentLoadTiming* GetDocumentLoadTiming() const;
  ResourceLoadTiming* GetResourceLoadTiming() const;
  InteractiveDetector* GetInteractiveDetector() const;
  absl::optional<base::TimeDelta> MonotonicTimeToPseudoWallTime(
      const absl::optional<base::TimeTicks>&) const;
  bool cross_origin_isolated_capability_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_TIMING_H_

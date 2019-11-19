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

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
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
class LocalFrame;
class PaintTiming;
class PaintTimingDetector;
class ResourceLoadTiming;
class ScriptState;
class ScriptValue;

// Legacy support for NT1(https://www.w3.org/TR/navigation-timing/).
class CORE_EXPORT PerformanceTiming final : public ScriptWrappable,
                                            public DOMWindowClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(PerformanceTiming);

 public:
  explicit PerformanceTiming(LocalFrame*);

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

  // The below are non-spec timings, for Page Load UMA metrics.

  // The time the first document layout is performed.
  uint64_t FirstLayout() const;
  // The time the first paint operation was performed.
  uint64_t FirstPaint() const;
  // The time the first paint operation for image was performed.
  uint64_t FirstImagePaint() const;
  // The time of the first 'contentful' paint. A contentful paint is a paint
  // that includes content of some kind (for example, text or image content).
  uint64_t FirstContentfulPaint() const;
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
  // The first time the page is considered 'interactive'. This is determined
  // using heuristics based on main thread and network activity.
  uint64_t PageInteractive() const;
  // The time of when we detect the page is interactive. There is a delay
  // between when the page was interactive and when we were able to detect it.
  uint64_t PageInteractiveDetection() const;
  // The time of when a significant input event happened that may cause
  // observers to discard the value of Time to Interactive.
  uint64_t FirstInputInvalidatingInteractive() const;
  // The duration between the hardware timestamp and being queued on the main
  // thread for the first click, tap, key press, cancellable touchstart, or
  // pointer down followed by a pointer up.
  uint64_t FirstInputDelay() const;
  // The timestamp of the event whose delay is reported by FirstInputDelay().
  uint64_t FirstInputTimestamp() const;
  // The longest duration between the hardware timestamp and being queued on the
  // main thread for the click, tap, key press, cancellable touchstart, or
  // pointer down followed by a pointer up.
  uint64_t LongestInputDelay() const;
  // The timestamp of the event whose delay is reported by LongestInputDelay().
  uint64_t LongestInputTimestamp() const;

  uint64_t ParseStart() const;
  uint64_t ParseStop() const;
  uint64_t ParseBlockedOnScriptLoadDuration() const;
  uint64_t ParseBlockedOnScriptLoadFromDocumentWriteDuration() const;
  uint64_t ParseBlockedOnScriptExecutionDuration() const;
  uint64_t ParseBlockedOnScriptExecutionFromDocumentWriteDuration() const;

  ScriptValue toJSONForBinding(ScriptState*) const;

  void Trace(blink::Visitor*) override;

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
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_TIMING_H_

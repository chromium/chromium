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
#include "third_party/blink/renderer/core/dom/context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

namespace blink {

class CSSTiming;
class DocumentLoadTiming;
class DocumentLoader;
class DocumentParserTiming;
class DocumentTiming;
class InteractiveDetector;
class LocalFrame;
class PaintTiming;
class PaintTracker;
class ResourceLoadTiming;
class ScriptState;
class ScriptValue;

// Legacy support for NT1(https://www.w3.org/TR/navigation-timing/).
class CORE_EXPORT PerformanceTiming final : public ScriptWrappable,
                                            public DOMWindowClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(PerformanceTiming);

 public:
  static PerformanceTiming* Create(LocalFrame* frame) {
    return new PerformanceTiming(frame);
  }

  unsigned long long navigationStart() const;
  unsigned long long inputStart() const;
  unsigned long long unloadEventStart() const;
  unsigned long long unloadEventEnd() const;
  unsigned long long redirectStart() const;
  unsigned long long redirectEnd() const;
  unsigned long long fetchStart() const;
  unsigned long long domainLookupStart() const;
  unsigned long long domainLookupEnd() const;
  unsigned long long connectStart() const;
  unsigned long long connectEnd() const;
  unsigned long long secureConnectionStart() const;
  unsigned long long requestStart() const;
  unsigned long long responseStart() const;
  unsigned long long responseEnd() const;
  unsigned long long domLoading() const;
  unsigned long long domInteractive() const;
  unsigned long long domContentLoadedEventStart() const;
  unsigned long long domContentLoadedEventEnd() const;
  unsigned long long domComplete() const;
  unsigned long long loadEventStart() const;
  unsigned long long loadEventEnd() const;

  // The below are non-spec timings, for Page Load UMA metrics.

  // The time the first document layout is performed.
  unsigned long long FirstLayout() const;
  // The time the first paint operation was performed.
  unsigned long long FirstPaint() const;
  // The time the first paint operation for visible text was performed.
  unsigned long long FirstTextPaint() const;
  // The time the first paint operation for image was performed.
  unsigned long long FirstImagePaint() const;
  // The time of the first 'contentful' paint. A contentful paint is a paint
  // that includes content of some kind (for example, text or image content).
  unsigned long long FirstContentfulPaint() const;
  // The time of the first 'meaningful' paint, A meaningful paint is a paint
  // where the page's primary content is visible.
  unsigned long long FirstMeaningfulPaint() const;
  // The time of the candidate of first 'meaningful' paint, A meaningful paint
  // candidate indicates the first time we considered a paint to qualify as the
  // potential first meaningful paint. But, be careful that it may be an
  // optimistic (i.e., too early) estimate.
  // TODO(crbug.com/848639): This function is exposed as an experiment, and if
  // not useful, this function can be removed.
  unsigned long long FirstMeaningfulPaintCandidate() const;
  // The time of the first paint after the largest image within viewport being
  // fully loaded.
  unsigned long long LargestImagePaint() const;
  // The time of the first paint after the last image within viewport being
  // fully loaded.
  unsigned long long LastImagePaint() const;
  // The time of the first paint of the largest text within viewport.
  unsigned long long LargestTextPaint() const;
  // The time of the first paint of the last text within viewport.
  unsigned long long LastTextPaint() const;
  // The first time the page is considered 'interactive'. This is determined
  // using heuristics based on main thread and network activity.
  unsigned long long PageInteractive() const;
  // The time of when we detect the page is interactive. There is a delay
  // between when the page was interactive and when we were able to detect it.
  unsigned long long PageInteractiveDetection() const;
  // The time of when a significant input event happened that may cause
  // observers to discard the value of Time to Interactive.
  unsigned long long FirstInputInvalidatingInteractive() const;
  // The duration between the hardware timestamp and being queued on the main
  // thread for the first click, tap, key press, cancellable touchstart, or
  // pointer down followed by a pointer up.
  unsigned long long FirstInputDelay() const;
  // The timestamp of the event whose delay is reported by FirstInputDelay().
  unsigned long long FirstInputTimestamp() const;
  // The longest duration between the hardware timestamp and being queued on the
  // main thread for the click, tap, key press, cancellable touchstart, or
  // pointer down followed by a pointer up.
  unsigned long long LongestInputDelay() const;
  // The timestamp of the event whose delay is reported by LongestInputDelay().
  unsigned long long LongestInputTimestamp() const;

  unsigned long long ParseStart() const;
  unsigned long long ParseStop() const;
  unsigned long long ParseBlockedOnScriptLoadDuration() const;
  unsigned long long ParseBlockedOnScriptLoadFromDocumentWriteDuration() const;
  unsigned long long ParseBlockedOnScriptExecutionDuration() const;
  unsigned long long ParseBlockedOnScriptExecutionFromDocumentWriteDuration()
      const;

  ScriptValue toJSONForBinding(ScriptState*) const;

  void Trace(blink::Visitor*) override;

  unsigned long long MonotonicTimeToIntegerMilliseconds(TimeTicks) const;

 private:
  explicit PerformanceTiming(LocalFrame*);

  const DocumentTiming* GetDocumentTiming() const;
  const CSSTiming* CssTiming() const;
  const DocumentParserTiming* GetDocumentParserTiming() const;
  const PaintTiming* GetPaintTiming() const;
  PaintTracker* GetPaintTracker() const;
  DocumentLoader* GetDocumentLoader() const;
  DocumentLoadTiming* GetDocumentLoadTiming() const;
  ResourceLoadTiming* GetResourceLoadTiming() const;
  InteractiveDetector* GetInteractiveDetector() const;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_TIMING_H_

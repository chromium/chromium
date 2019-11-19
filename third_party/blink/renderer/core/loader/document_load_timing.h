/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_DOCUMENT_LOAD_TIMING_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_DOCUMENT_LOAD_TIMING_H_

#include "base/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"

namespace base {
class Clock;
class TickClock;
}  // namespace base

namespace blink {

class DocumentLoader;
class KURL;
class LocalFrame;

class CORE_EXPORT DocumentLoadTiming final {
  DISALLOW_NEW();

 public:
  explicit DocumentLoadTiming(DocumentLoader&);

  base::TimeDelta MonotonicTimeToZeroBasedDocumentTime(base::TimeTicks) const;
  base::TimeDelta MonotonicTimeToPseudoWallTime(base::TimeTicks) const;

  void MarkNavigationStart();
  void SetNavigationStart(base::TimeTicks);

  void SetInputStart(base::TimeTicks);

  void AddRedirect(const KURL& redirecting_url, const KURL& redirected_url);
  void SetRedirectStart(base::TimeTicks);
  void SetRedirectEnd(base::TimeTicks);
  void SetRedirectCount(uint16_t value) { redirect_count_ = value; }
  void SetHasCrossOriginRedirect(bool value) {
    has_cross_origin_redirect_ = value;
  }

  void MarkUnloadEventStart(base::TimeTicks);
  void MarkUnloadEventEnd(base::TimeTicks);

  void MarkFetchStart();
  void SetFetchStart(base::TimeTicks);

  void SetResponseEnd(base::TimeTicks);

  void MarkLoadEventStart();
  void MarkLoadEventEnd();

  void SetHasSameOriginAsPreviousDocument(bool value) {
    has_same_origin_as_previous_document_ = value;
  }

  base::TimeTicks InputStart() const { return input_start_; }
  base::TimeTicks NavigationStart() const { return navigation_start_; }
  base::TimeTicks UnloadEventStart() const { return unload_event_start_; }
  base::TimeTicks UnloadEventEnd() const { return unload_event_end_; }
  base::TimeTicks RedirectStart() const { return redirect_start_; }
  base::TimeTicks RedirectEnd() const { return redirect_end_; }
  uint16_t RedirectCount() const { return redirect_count_; }
  base::TimeTicks FetchStart() const { return fetch_start_; }
  base::TimeTicks ResponseEnd() const { return response_end_; }
  base::TimeTicks LoadEventStart() const { return load_event_start_; }
  base::TimeTicks LoadEventEnd() const { return load_event_end_; }
  bool HasCrossOriginRedirect() const { return has_cross_origin_redirect_; }
  bool HasSameOriginAsPreviousDocument() const {
    return has_same_origin_as_previous_document_;
  }

  base::TimeTicks ReferenceMonotonicTime() const {
    return reference_monotonic_time_;
  }

  void Trace(blink::Visitor*);

  void SetTickClockForTesting(const base::TickClock* tick_clock);
  void SetClockForTesting(const base::Clock* clock);

 private:
  void MarkRedirectEnd();
  void NotifyDocumentTimingChanged();
  void EnsureReferenceTimesSet();
  LocalFrame* GetFrame() const;
  std::unique_ptr<TracedValue> GetNavigationStartTracingData() const;

  base::TimeTicks reference_monotonic_time_;
  base::TimeDelta reference_wall_time_;
  base::TimeTicks input_start_;
  base::TimeTicks navigation_start_;
  base::TimeTicks unload_event_start_;
  base::TimeTicks unload_event_end_;
  base::TimeTicks redirect_start_;
  base::TimeTicks redirect_end_;
  uint16_t redirect_count_;
  base::TimeTicks fetch_start_;
  base::TimeTicks response_end_;
  base::TimeTicks load_event_start_;
  base::TimeTicks load_event_end_;
  bool has_cross_origin_redirect_;
  bool has_same_origin_as_previous_document_;

  const base::Clock* clock_;
  const base::TickClock* tick_clock_;

  Member<DocumentLoader> document_loader_;
};

}  // namespace blink

#endif

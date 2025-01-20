// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/document_timing.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/interactive_detector.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

DocumentTiming::DocumentTiming(Document& document) : document_(document) {
  document_timing_values_ = MakeGarbageCollected<DocumentTimingValues>();
  if (document_->GetReadyState() == Document::kLoading)
    MarkDomLoading();
}

void DocumentTiming::Trace(Visitor* visitor) const {
  visitor->Trace(document_);
  visitor->Trace(document_timing_values_);
}

LocalFrame* DocumentTiming::GetFrame() const {
  return document_ ? document_->GetFrame() : nullptr;
}

void DocumentTiming::NotifyDocumentTimingChanged() {
  if (document_ && document_->Loader())
    document_->Loader()->DidChangePerformanceTiming();
}

void DocumentTiming::MarkDomLoading() {
  document_timing_values_->dom_loading = base::TimeTicks::Now();
  TRACE_EVENT_MARK_WITH_TIMESTAMP1("blink.user_timing,rail", "domLoading",
                                   document_timing_values_->dom_loading,
                                   "frame", GetFrameIdForTracing(GetFrame()));
  NotifyDocumentTimingChanged();
}

void DocumentTiming::MarkDomInteractive() {
  document_timing_values_->dom_interactive = base::TimeTicks::Now();
  TRACE_EVENT_MARK_WITH_TIMESTAMP1("blink.user_timing,rail", "domInteractive",
                                   document_timing_values_->dom_interactive,
                                   "frame", GetFrameIdForTracing(GetFrame()));
  NotifyDocumentTimingChanged();
}

void DocumentTiming::MarkDomContentLoadedEventStart() {
  document_timing_values_->dom_content_loaded_event_start =
      base::TimeTicks::Now();
  TRACE_EVENT_MARK_WITH_TIMESTAMP1(
      "blink.user_timing,rail", "domContentLoadedEventStart",
      document_timing_values_->dom_content_loaded_event_start, "frame",
      GetFrameIdForTracing(GetFrame()));
  NotifyDocumentTimingChanged();
}

void DocumentTiming::MarkDomContentLoadedEventEnd() {
  document_timing_values_->dom_content_loaded_event_end =
      base::TimeTicks::Now();
  TRACE_EVENT_MARK_WITH_TIMESTAMP1(
      "blink.user_timing,rail", "domContentLoadedEventEnd",
      document_timing_values_->dom_content_loaded_event_end, "frame",
      GetFrameIdForTracing(GetFrame()));
  InteractiveDetector* interactive_detector(
      InteractiveDetector::From(*document_));
  if (interactive_detector) {
    interactive_detector->OnDomContentLoadedEnd(
        document_timing_values_->dom_content_loaded_event_end);
  }
  NotifyDocumentTimingChanged();
}

void DocumentTiming::MarkDomComplete() {
  document_timing_values_->dom_complete = base::TimeTicks::Now();
  TRACE_EVENT_MARK_WITH_TIMESTAMP1("blink.user_timing,rail", "domComplete",
                                   document_timing_values_->dom_complete,
                                   "frame", GetFrameIdForTracing(GetFrame()));
  NotifyDocumentTimingChanged();
}

}  // namespace blink

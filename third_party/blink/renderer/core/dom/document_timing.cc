// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/document_timing.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/interactive_detector.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

DocumentTiming::DocumentTiming(Document& document) : document_(document) {}

void DocumentTiming::Trace(Visitor* visitor) {
  visitor->Trace(document_);
}

LocalFrame* DocumentTiming::GetFrame() const {
  return document_ ? document_->GetFrame() : nullptr;
}

void DocumentTiming::NotifyDocumentTimingChanged() {
  if (document_ && document_->Loader())
    document_->Loader()->DidChangePerformanceTiming();
}

void DocumentTiming::MarkDomLoading() {
  dom_loading_ = base::TimeTicks::Now();
  TRACE_EVENT_MARK_WITH_TIMESTAMP1("blink.user_timing,rail", "domLoading",
                                   dom_loading_, "frame",
                                   ToTraceValue(GetFrame()));
  NotifyDocumentTimingChanged();
}

void DocumentTiming::MarkDomInteractive() {
  dom_interactive_ = base::TimeTicks::Now();
  TRACE_EVENT_MARK_WITH_TIMESTAMP1("blink.user_timing,rail", "domInteractive",
                                   dom_interactive_, "frame",
                                   ToTraceValue(GetFrame()));
  NotifyDocumentTimingChanged();
}

void DocumentTiming::MarkDomContentLoadedEventStart() {
  dom_content_loaded_event_start_ = base::TimeTicks::Now();
  TRACE_EVENT_MARK_WITH_TIMESTAMP1(
      "blink.user_timing,rail", "domContentLoadedEventStart",
      dom_content_loaded_event_start_, "frame", ToTraceValue(GetFrame()));
  NotifyDocumentTimingChanged();
}

void DocumentTiming::MarkDomContentLoadedEventEnd() {
  dom_content_loaded_event_end_ = base::TimeTicks::Now();
  TRACE_EVENT_MARK_WITH_TIMESTAMP1(
      "blink.user_timing,rail", "domContentLoadedEventEnd",
      dom_content_loaded_event_end_, "frame", ToTraceValue(GetFrame()));
  InteractiveDetector* interactive_detector(
      InteractiveDetector::From(*document_));
  if (interactive_detector) {
    interactive_detector->OnDomContentLoadedEnd(dom_content_loaded_event_end_);
  }
  NotifyDocumentTimingChanged();
}

void DocumentTiming::MarkDomComplete() {
  dom_complete_ = base::TimeTicks::Now();
  TRACE_EVENT_MARK_WITH_TIMESTAMP1("blink.user_timing,rail", "domComplete",
                                   dom_complete_, "frame",
                                   ToTraceValue(GetFrame()));
  NotifyDocumentTimingChanged();
}

void DocumentTiming::MarkFirstLayout() {
  first_layout_ = base::TimeTicks::Now();
  TRACE_EVENT_MARK_WITH_TIMESTAMP1("blink.user_timing,rail", "firstLayout",
                                   first_layout_, "frame",
                                   ToTraceValue(GetFrame()));
  NotifyDocumentTimingChanged();
}

}  // namespace blink

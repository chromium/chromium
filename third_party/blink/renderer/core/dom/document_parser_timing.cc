// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/document_parser_timing.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

// static
const char DocumentParserTiming::kSupplementName[] = "DocumentParserTiming";

DocumentParserTiming& DocumentParserTiming::From(Document& document) {
  DocumentParserTiming* timing =
      Supplement<Document>::From<DocumentParserTiming>(document);
  if (!timing) {
    timing = MakeGarbageCollected<DocumentParserTiming>(document);
    ProvideTo(document, timing);
  }
  return *timing;
}

void DocumentParserTiming::MarkParserStart() {
  if (parser_detached_ || !parser_start_.is_null())
    return;
  DCHECK(parser_stop_.is_null());
  parser_start_ = base::TimeTicks::Now();
  NotifyDocumentParserTimingChanged();
}

void DocumentParserTiming::MarkParserStop() {
  if (parser_detached_ || parser_start_.is_null() || !parser_stop_.is_null())
    return;
  parser_stop_ = base::TimeTicks::Now();
  NotifyDocumentParserTimingChanged();
}

void DocumentParserTiming::MarkParserDetached() {
  DCHECK(!parser_start_.is_null());
  parser_detached_ = true;
}

void DocumentParserTiming::RecordParserBlockedOnScriptLoadDuration(
    base::TimeDelta duration,
    bool script_inserted_via_document_write) {
  if (parser_detached_ || parser_start_.is_null() || !parser_stop_.is_null())
    return;
  parser_blocked_on_script_load_duration_ += duration;
  if (script_inserted_via_document_write)
    parser_blocked_on_script_load_from_document_write_duration_ += duration;
  NotifyDocumentParserTimingChanged();
}

void DocumentParserTiming::RecordParserBlockedOnScriptExecutionDuration(
    base::TimeDelta duration,
    bool script_inserted_via_document_write) {
  if (parser_detached_ || parser_start_.is_null() || !parser_stop_.is_null())
    return;
  parser_blocked_on_script_execution_duration_ += duration;
  if (script_inserted_via_document_write)
    parser_blocked_on_script_execution_from_document_write_duration_ +=
        duration;
  NotifyDocumentParserTimingChanged();
}

void DocumentParserTiming::Trace(Visitor* visitor) const {
  Supplement<Document>::Trace(visitor);
}

DocumentParserTiming::DocumentParserTiming(Document& document)
    : Supplement<Document>(document) {}

void DocumentParserTiming::NotifyDocumentParserTimingChanged() {
  if (GetSupplementable()->Loader())
    GetSupplementable()->Loader()->DidChangePerformanceTiming();
}

}  // namespace blink

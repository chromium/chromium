// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_REPORTING_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_REPORTING_CONTEXT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class ExecutionContext;
class Report;
class ReportingObserver;

// ReportingContext is used as a container for all active ReportingObservers for
// an ExecutionContext.
class CORE_EXPORT ReportingContext final
    : public GarbageCollectedFinalized<ReportingContext>,
      public Supplement<ExecutionContext> {
  USING_GARBAGE_COLLECTED_MIXIN(ReportingContext)
 public:
  static const char kSupplementName[];

  explicit ReportingContext(ExecutionContext&);

  // Returns the ReportingContext for an ExecutionContext. If one does not
  // already exist for the given context, one is created.
  static ReportingContext* From(ExecutionContext*);
  static ReportingContext* From(const ExecutionContext* context) {
    return ReportingContext::From(const_cast<ExecutionContext*>(context));
  }

  // Queues a report in all registered observers.
  void QueueReport(Report*);

  // Counts the use of a report type via UseCounter.
  void CountReport(Report*);

  void RegisterObserver(ReportingObserver*);
  void UnregisterObserver(ReportingObserver*);

  void Trace(blink::Visitor*) override;

 private:
  HeapListHashSet<Member<ReportingObserver>> observers_;
  HeapListHashSet<Member<Report>> report_buffer_;
  Member<ExecutionContext> execution_context_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_REPORTING_CONTEXT_H_

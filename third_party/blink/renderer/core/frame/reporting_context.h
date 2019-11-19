// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_REPORTING_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_REPORTING_CONTEXT_H_

#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/reporting/reporting.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ExecutionContext;
class Report;
class ReportingObserver;

// ReportingContext processes all reports for an ExecutionContext, and serves as
// a container for all active ReportingObservers on that ExecutionContext.
class CORE_EXPORT ReportingContext final
    : public GarbageCollected<ReportingContext>,
      public Supplement<ExecutionContext> {
  USING_GARBAGE_COLLECTED_MIXIN(ReportingContext);

 public:
  static const char kSupplementName[];

  explicit ReportingContext(ExecutionContext&);

  // Returns the ReportingContext for an ExecutionContext. If one does not
  // already exist for the given context, one is created.
  static ReportingContext* From(ExecutionContext*);
  static ReportingContext* From(const ExecutionContext* context) {
    return ReportingContext::From(const_cast<ExecutionContext*>(context));
  }

  // Queues a report for the Reporting API and in all registered observers.
  void QueueReport(Report*, const Vector<String>& endpoints = {"default"});

  void RegisterObserver(ReportingObserver*);
  void UnregisterObserver(ReportingObserver*);

  void Trace(blink::Visitor*) override;

 private:
  // Counts the use of a report type via UseCounter.
  void CountReport(Report*);

  const mojo::Remote<mojom::blink::ReportingServiceProxy>& GetReportingService()
      const;

  // Send |report| via the Reporting API to |endpoint|.
  void SendToReportingAPI(Report* report, const String& endpoint) const;

  HeapListHashSet<Member<ReportingObserver>> observers_;
  HeapHashMap<String, HeapListHashSet<Member<Report>>> report_buffer_;
  Member<ExecutionContext> execution_context_;

  // This is declared mutable so that the service endpoint can be cached by
  // const methods.
  mutable mojo::Remote<mojom::blink::ReportingServiceProxy> reporting_service_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_REPORTING_CONTEXT_H_

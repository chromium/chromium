// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/reporting_context.h"

#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/csp/csp_violation_report_body.h"
#include "third_party/blink/renderer/core/frame/deprecation_report_body.h"
#include "third_party/blink/renderer/core/frame/feature_policy_violation_report_body.h"
#include "third_party/blink/renderer/core/frame/intervention_report_body.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/report.h"
#include "third_party/blink/renderer/core/frame/reporting_observer.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

// static
const char ReportingContext::kSupplementName[] = "ReportingContext";

ReportingContext::ReportingContext(ExecutionContext& context)
    : Supplement<ExecutionContext>(context), execution_context_(context) {}

// static
ReportingContext* ReportingContext::From(ExecutionContext* context) {
  ReportingContext* reporting_context =
      Supplement<ExecutionContext>::From<ReportingContext>(context);
  if (!reporting_context) {
    reporting_context = MakeGarbageCollected<ReportingContext>(*context);
    Supplement<ExecutionContext>::ProvideTo(*context, reporting_context);
  }
  return reporting_context;
}

void ReportingContext::QueueReport(Report* report,
                                   const Vector<String>& endpoints) {
  CountReport(report);

  // Buffer the report.
  if (!report_buffer_.Contains(report->type()))
    report_buffer_.insert(report->type(), HeapListHashSet<Member<Report>>());
  report_buffer_.find(report->type())->value.insert(report);

  // Only the most recent 100 reports will remain buffered, per report type.
  // https://w3c.github.io/reporting/#notify-observers
  if (report_buffer_.at(report->type()).size() > 100)
    report_buffer_.find(report->type())->value.RemoveFirst();

  // Queue the report in all registered observers.
  for (auto observer : observers_)
    observer->QueueReport(report);

  // Send the report via the Reporting API.
  for (auto& endpoint : endpoints)
    SendToReportingAPI(report, endpoint);
}

void ReportingContext::RegisterObserver(ReportingObserver* observer) {
  UseCounter::Count(execution_context_, WebFeature::kReportingObserver);

  observers_.insert(observer);
  if (!observer->Buffered())
    return;

  observer->ClearBuffered();
  for (auto type : report_buffer_) {
    for (Report* report : type.value) {
      observer->QueueReport(report);
    }
  }
}

void ReportingContext::UnregisterObserver(ReportingObserver* observer) {
  observers_.erase(observer);
}

void ReportingContext::Trace(blink::Visitor* visitor) {
  visitor->Trace(observers_);
  visitor->Trace(report_buffer_);
  visitor->Trace(execution_context_);
  Supplement<ExecutionContext>::Trace(visitor);
}

void ReportingContext::CountReport(Report* report) {
  const String& type = report->type();
  WebFeature feature;

  if (type == ReportType::kDeprecation) {
    feature = WebFeature::kDeprecationReport;
  } else if (type == ReportType::kFeaturePolicyViolation) {
    feature = WebFeature::kFeaturePolicyReport;
  } else if (type == ReportType::kIntervention) {
    feature = WebFeature::kInterventionReport;
  } else {
    return;
  }

  UseCounter::Count(execution_context_, feature);
}

const mojo::Remote<mojom::blink::ReportingServiceProxy>&
ReportingContext::GetReportingService() const {
  if (!reporting_service_) {
    Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
        reporting_service_.BindNewPipeAndPassReceiver());
  }
  return reporting_service_;
}

void ReportingContext::SendToReportingAPI(Report* report,
                                          const String& endpoint) const {
  const String& type = report->type();
  if (!(type == ReportType::kCSPViolation || type == ReportType::kDeprecation ||
        type == ReportType::kFeaturePolicyViolation ||
        type == ReportType::kIntervention)) {
    return;
  }

  const LocationReportBody* location_body =
      static_cast<LocationReportBody*>(report->body());
  bool is_null;
  int line_number = location_body->lineNumber(is_null);
  if (is_null)
    line_number = 0;
  int column_number = location_body->columnNumber(is_null);
  if (is_null)
    column_number = 0;
  KURL url = KURL(report->url());

  if (type == ReportType::kCSPViolation) {
    // Send the CSP violation report.
    const CSPViolationReportBody* body =
        static_cast<CSPViolationReportBody*>(report->body());
    GetReportingService()->QueueCspViolationReport(
        url,
        endpoint,
        body->documentURL() ? body->documentURL() : "",
        body->referrer(),
        body->blockedURL(),
        body->effectiveDirective() ? body->effectiveDirective() : "",
        body->originalPolicy() ? body->originalPolicy() : "",
        body->sourceFile(),
        body->sample(),
        body->disposition() ? body->disposition() : "",
        body->statusCode(),
        line_number,
        column_number);
  } else if (type == ReportType::kDeprecation) {
    // Send the deprecation report.
    const DeprecationReportBody* body =
        static_cast<DeprecationReportBody*>(report->body());
    base::Optional<base::Time> anticipated_removal =
        base::Time::FromDoubleT(body->anticipatedRemoval(is_null));
    if (is_null)
      anticipated_removal = base::nullopt;
    GetReportingService()->QueueDeprecationReport(
        url, body->id(), anticipated_removal, body->message(),
        body->sourceFile(), line_number, column_number);
  } else if (type == ReportType::kFeaturePolicyViolation) {
    // Send the feature policy violation report.
    const FeaturePolicyViolationReportBody* body =
        static_cast<FeaturePolicyViolationReportBody*>(report->body());
    GetReportingService()->QueueFeaturePolicyViolationReport(
        url, body->featureId(), body->disposition(), "Feature policy violation",
        body->sourceFile(), line_number, column_number);
  } else if (type == ReportType::kIntervention) {
    // Send the intervention report.
    const InterventionReportBody* body =
        static_cast<InterventionReportBody*>(report->body());
    GetReportingService()->QueueInterventionReport(
        url, body->id(), body->message(), body->sourceFile(), line_number,
        column_number);
  }
}

}  // namespace blink

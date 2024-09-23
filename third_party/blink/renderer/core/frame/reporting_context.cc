// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/reporting_context.h"

#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/csp/csp_violation_report_body.h"
#include "third_party/blink/renderer/core/frame/deprecation/deprecation_report_body.h"
#include "third_party/blink/renderer/core/frame/document_policy_violation_report_body.h"
#include "third_party/blink/renderer/core/frame/intervention_report_body.h"
#include "third_party/blink/renderer/core/frame/permissions_policy_violation_report_body.h"
#include "third_party/blink/renderer/core/frame/report.h"
#include "third_party/blink/renderer/core/frame/reporting_observer.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

namespace {

// In the spec (https://w3c.github.io/reporting/#report-body) a report body can
// have anything that can be serialized into a JSON text, but V8ObjectBuilder
// doesn't allow us to implement that. Hence here we implement just a one-level
// dictionary, as that is what is needed currently.
class DictionaryValueReportBody final : public ReportBody {
 public:
  explicit DictionaryValueReportBody(mojom::blink::ReportBodyPtr body)
      : body_(std::move(body)) {}

  void BuildJSONValue(V8ObjectBuilder& builder) const override {
    DCHECK(body_);

    for (const auto& element : body_->body) {
      builder.AddString(element->name, element->value);
    }
  }

 private:
  const mojom::blink::ReportBodyPtr body_;
};

}  // namespace

// static
const char ReportingContext::kSupplementName[] = "ReportingContext";

ReportingContext::ReportingContext(ExecutionContext& context)
    : Supplement<ExecutionContext>(context),
      execution_context_(context),
      reporting_service_(&context),
      receiver_(this, &context) {}

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

void ReportingContext::Bind(
    mojo::PendingReceiver<mojom::blink::ReportingObserver> receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(receiver),
                 execution_context_->GetTaskRunner(TaskType::kMiscPlatformAPI));
}

void ReportingContext::QueueReport(Report* report,
                                   const Vector<String>& endpoints) {
  CountReport(report);

  NotifyInternal(report);

  // Send the report via the Reporting API.
  for (auto& endpoint : endpoints)
    SendToReportingAPI(report, endpoint);
}

void ReportingContext::RegisterObserver(blink::ReportingObserver* observer) {
  UseCounter::Count(execution_context_, WebFeature::kReportingObserver);

  observers_.insert(observer);
  if (!observer->Buffered())
    return;

  observer->ClearBuffered();
  for (auto type : report_buffer_) {
    for (Report* report : *type.value) {
      observer->QueueReport(report);
    }
  }
}

void ReportingContext::UnregisterObserver(blink::ReportingObserver* observer) {
  observers_.erase(observer);
}

void ReportingContext::Notify(mojom::blink::ReportPtr report) {
  ReportBody* body = report->body
                         ? MakeGarbageCollected<DictionaryValueReportBody>(
                               std::move(report->body))
                         : nullptr;
  NotifyInternal(MakeGarbageCollected<Report>(report->type,
                                              report->url.GetString(), body));
}

void ReportingContext::Trace(Visitor* visitor) const {
  visitor->Trace(observers_);
  visitor->Trace(report_buffer_);
  visitor->Trace(execution_context_);
  visitor->Trace(reporting_service_);
  visitor->Trace(receiver_);
  Supplement<ExecutionContext>::Trace(visitor);
}

void ReportingContext::CountReport(Report* report) {
  const String& type = report->type();
  WebFeature feature;

  if (type == ReportType::kDeprecation) {
    feature = WebFeature::kDeprecationReport;
  } else if (type == ReportType::kPermissionsPolicyViolation) {
    feature = WebFeature::kFeaturePolicyReport;
  } else if (type == ReportType::kIntervention) {
    feature = WebFeature::kInterventionReport;
  } else {
    return;
  }

  UseCounter::Count(execution_context_, feature);
}

const HeapMojoRemote<mojom::blink::ReportingServiceProxy>&
ReportingContext::GetReportingService() const {
  if (!reporting_service_.is_bound()) {
    execution_context_->GetBrowserInterfaceBroker().GetInterface(
        reporting_service_.BindNewPipeAndPassReceiver(
            execution_context_->GetTaskRunner(TaskType::kMiscPlatformAPI)));
  }
  return reporting_service_;
}

void ReportingContext::NotifyInternal(Report* report) {
  // Buffer the report.
  if (!report_buffer_.Contains(report->type())) {
    report_buffer_.insert(
        report->type(),
        MakeGarbageCollected<HeapLinkedHashSet<Member<Report>>>());
  }
  report_buffer_.find(report->type())->value->insert(report);

  // Only the most recent 100 reports will remain buffered, per report type.
  // https://w3c.github.io/reporting/#notify-observers
  if (report_buffer_.at(report->type())->size() > 100)
    report_buffer_.find(report->type())->value->RemoveFirst();

  // Queue the report in all registered observers.
  for (auto observer : observers_)
    observer->QueueReport(report);
}

void ReportingContext::SendToReportingAPI(Report* report,
                                          const String& endpoint) const {
  const String& type = report->type();
  if (!(type == ReportType::kCSPViolation || type == ReportType::kDeprecation ||
        type == ReportType::kPermissionsPolicyViolation ||
        type == ReportType::kIntervention ||
        type == ReportType::kDocumentPolicyViolation)) {
    return;
  }

  const LocationReportBody* location_body =
      static_cast<LocationReportBody*>(report->body());
  int line_number = location_body->lineNumber().value_or(0);
  int column_number = location_body->columnNumber().value_or(0);
  KURL url = KURL(report->url());

  if (type == ReportType::kCSPViolation) {
    // Send the CSP violation report.
    const CSPViolationReportBody* body =
        static_cast<CSPViolationReportBody*>(report->body());
    GetReportingService()->QueueCspViolationReport(
        url, endpoint, body->documentURL() ? body->documentURL() : "",
        body->referrer(), body->blockedURL(),
        body->effectiveDirective() ? body->effectiveDirective() : "",
        body->originalPolicy() ? body->originalPolicy() : "",
        body->sourceFile(), body->sample(),
        body->disposition() ? body->disposition() : "", body->statusCode(),
        line_number, column_number);
  } else if (type == ReportType::kDeprecation) {
    // Send the deprecation report.
    const DeprecationReportBody* body =
        static_cast<DeprecationReportBody*>(report->body());
    GetReportingService()->QueueDeprecationReport(
        url, body->id(), body->AnticipatedRemoval(),
        body->message().IsNull() ? g_empty_string : body->message(),
        body->sourceFile(), line_number, column_number);
  } else if (type == ReportType::kPermissionsPolicyViolation) {
    // Send the permissions policy violation report.
    const PermissionsPolicyViolationReportBody* body =
        static_cast<PermissionsPolicyViolationReportBody*>(report->body());
    GetReportingService()->QueuePermissionsPolicyViolationReport(
        url, endpoint, body->featureId(), body->disposition(), body->message(),
        body->sourceFile(), line_number, column_number);
  } else if (type == ReportType::kIntervention) {
    // Send the intervention report.
    const InterventionReportBody* body =
        static_cast<InterventionReportBody*>(report->body());
    GetReportingService()->QueueInterventionReport(
        url, body->id(),
        body->message().IsNull() ? g_empty_string : body->message(),
        body->sourceFile(), line_number, column_number);
  } else if (type == ReportType::kDocumentPolicyViolation) {
    const DocumentPolicyViolationReportBody* body =
        static_cast<DocumentPolicyViolationReportBody*>(report->body());
    // Send the document policy violation report.
    GetReportingService()->QueueDocumentPolicyViolationReport(
        url, endpoint, body->featureId(), body->disposition(), body->message(),
        body->sourceFile(), line_number, column_number);
  }
}

}  // namespace blink

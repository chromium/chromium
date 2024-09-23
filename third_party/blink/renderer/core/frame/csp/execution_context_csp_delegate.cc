// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/csp/execution_context_csp_delegate.h"

#include "services/network/public/cpp/web_sandbox_flags.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom-blink.h"
#include "third_party/blink/public/common/security_context/insecure_request_policy.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/capture_source_location.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/events/security_policy_violation_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/csp/csp_violation_report_body.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/report.h"
#include "third_party/blink/renderer/core/frame/reporting_context.h"
#include "third_party/blink/renderer/core/inspector/inspector_audits_issue.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/ping_loader.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worklet_global_scope.h"
#include "third_party/blink/renderer/platform/bindings/source_location.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/network/encoded_form_data.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

ExecutionContextCSPDelegate::ExecutionContextCSPDelegate(
    ExecutionContext& execution_context)
    : execution_context_(&execution_context) {}

void ExecutionContextCSPDelegate::Trace(Visitor* visitor) const {
  visitor->Trace(execution_context_);
  ContentSecurityPolicyDelegate::Trace(visitor);
}

const SecurityOrigin* ExecutionContextCSPDelegate::GetSecurityOrigin() {
  return execution_context_->GetSecurityOrigin();
}

const KURL& ExecutionContextCSPDelegate::Url() const {
  return execution_context_->Url();
}

void ExecutionContextCSPDelegate::SetSandboxFlags(
    network::mojom::blink::WebSandboxFlags mask) {
  // Ideally sandbox flags are determined at construction time since
  // sandbox flags influence the security origin and that influences
  // the Agent that is assigned for the ExecutionContext. Changing
  // an ExecutionContext's agent in the middle of an object lifecycle
  // is not permitted.

  // Since Workers and Worklets don't share agents (each one is unique)
  // we allow them to apply new sandbox flags on top of the current ones.
  WorkerOrWorkletGlobalScope* worklet_or_worker =
      DynamicTo<WorkerOrWorkletGlobalScope>(execution_context_.Get());
  if (worklet_or_worker) {
    worklet_or_worker->SetSandboxFlags(mask);
  }
  // Just check that all the sandbox flags that are set by CSP have
  // already been set on the security context. Meta tags can't set them
  // and we should have already constructed the document with the correct
  // sandbox flags from CSP already.
  network::mojom::blink::WebSandboxFlags flags =
      execution_context_->GetSandboxFlags();
  CHECK_EQ(flags | mask, flags);
}

void ExecutionContextCSPDelegate::SetRequireTrustedTypes() {
  execution_context_->SetRequireTrustedTypes();
}

void ExecutionContextCSPDelegate::AddInsecureRequestPolicy(
    mojom::blink::InsecureRequestPolicy policy) {
  SecurityContext& security_context = GetSecurityContext();

  auto* window = DynamicTo<LocalDOMWindow>(execution_context_.Get());

  // Step 2. Set settings’s insecure requests policy to Upgrade. [spec text]
  // Upgrade Insecure Requests: Update the policy.
  security_context.SetInsecureRequestPolicy(
      security_context.GetInsecureRequestPolicy() | policy);
  if (window && window->GetFrame()) {
    window->GetFrame()->GetLocalFrameHostRemote().EnforceInsecureRequestPolicy(
        security_context.GetInsecureRequestPolicy());
  }

  // Upgrade Insecure Requests: Update the set of insecure URLs to upgrade.
  if ((policy &
       mojom::blink::InsecureRequestPolicy::kUpgradeInsecureRequests) !=
      mojom::blink::InsecureRequestPolicy::kLeaveInsecureRequestsAlone) {
    // Spec: Enforcing part of:
    // https://w3c.github.io/webappsec-upgrade-insecure-requests/#delivery
    // Step 3. Let tuple be a tuple of the protected resource’s URL's host and
    // port. [spec text]
    // Step 4. Insert tuple into settings’s upgrade insecure navigations set.
    // [spec text]
    Count(WebFeature::kUpgradeInsecureRequestsEnabled);
    // We don't add the hash if |window| is null, to prevent
    // WorkerGlobalScope::Url() before it's ready. https://crbug.com/861564
    // This should be safe, because the insecure navigations set is not used
    // in non-Document contexts.
    if (window && !Url().Host().empty()) {
      uint32_t hash = Url().Host().ToString().Impl()->GetHash();
      security_context.AddInsecureNavigationUpgrade(hash);
      if (auto* frame = window->GetFrame()) {
        frame->GetLocalFrameHostRemote().EnforceInsecureNavigationsSet(
            SecurityContext::SerializeInsecureNavigationSet(
                GetSecurityContext().InsecureNavigationsToUpgrade()));
      }
    }
  }
}

std::unique_ptr<SourceLocation>
ExecutionContextCSPDelegate::GetSourceLocation() {
  return CaptureSourceLocation(execution_context_);
}

std::optional<uint16_t> ExecutionContextCSPDelegate::GetStatusCode() {
  std::optional<uint16_t> status_code;

  // TODO(mkwst): We only have status code information for Documents. It would
  // be nice to get them for Workers as well.
  Document* document = GetDocument();
  if (document && document->Loader())
    status_code = document->Loader()->GetResponse().HttpStatusCode();

  return status_code;
}

String ExecutionContextCSPDelegate::GetDocumentReferrer() {
  String referrer;

  // TODO(mkwst): We only have referrer information for Documents. It would be
  // nice to get them for Workers as well.
  if (Document* document = GetDocument())
    referrer = document->referrer();
  return referrer;
}

void ExecutionContextCSPDelegate::DispatchViolationEvent(
    const SecurityPolicyViolationEventInit& violation_data,
    Element* element) {
  execution_context_->GetTaskRunner(TaskType::kNetworking)
      ->PostTask(
          FROM_HERE,
          WTF::BindOnce(
              &ExecutionContextCSPDelegate::DispatchViolationEventInternal,
              WrapPersistent(this), WrapPersistent(&violation_data),
              WrapPersistent(element)));
}

void ExecutionContextCSPDelegate::PostViolationReport(
    const SecurityPolicyViolationEventInit& violation_data,
    const String& stringified_report,
    bool is_frame_ancestors_violation,
    const Vector<String>& report_endpoints,
    bool use_reporting_api) {
  DCHECK_EQ(is_frame_ancestors_violation,
            network::mojom::blink::CSPDirectiveName::FrameAncestors ==
                ContentSecurityPolicy::GetDirectiveType(
                    violation_data.effectiveDirective()));

  // We do not support reporting for worklets, since they don't have a
  // ResourceFetcher.
  //
  // TODO(https://crbug.com/1222576): Send CSP reports for worklets using the
  // owner document's ResourceFetcher.
  if (DynamicTo<WorkletGlobalScope>(execution_context_.Get()))
    return;

  scoped_refptr<EncodedFormData> report =
      EncodedFormData::Create(stringified_report.Utf8());

  // Construct and route the report to the ReportingContext, to be observed
  // by any ReportingObservers.
  auto* body = MakeGarbageCollected<CSPViolationReportBody>(violation_data);
  String url_sending_report = is_frame_ancestors_violation
                                  ? violation_data.documentURI()
                                  : Url().GetString();
  Report* observed_report = MakeGarbageCollected<Report>(
      ReportType::kCSPViolation, url_sending_report, body);
  ReportingContext::From(execution_context_.Get())
      ->QueueReport(observed_report,
                    use_reporting_api ? report_endpoints : Vector<String>());

  if (use_reporting_api)
    return;

  for (const auto& report_endpoint : report_endpoints) {
    PingLoader::SendViolationReport(execution_context_.Get(),
                                    KURL(report_endpoint), report,
                                    is_frame_ancestors_violation);
  }
}

void ExecutionContextCSPDelegate::Count(WebFeature feature) {
  UseCounter::Count(execution_context_, feature);
}

void ExecutionContextCSPDelegate::AddConsoleMessage(
    ConsoleMessage* console_message) {
  execution_context_->AddConsoleMessage(console_message);
}

void ExecutionContextCSPDelegate::AddInspectorIssue(AuditsIssue issue) {
  execution_context_->AddInspectorIssue(std::move(issue));
}

void ExecutionContextCSPDelegate::DisableEval(const String& error_message) {
  execution_context_->DisableEval(error_message);
}

void ExecutionContextCSPDelegate::SetWasmEvalErrorMessage(
    const String& error_message) {
  execution_context_->SetWasmEvalErrorMessage(error_message);
}

void ExecutionContextCSPDelegate::ReportBlockedScriptExecutionToInspector(
    const String& directive_text) {
  probe::ScriptExecutionBlockedByCSP(execution_context_, directive_text);
}

void ExecutionContextCSPDelegate::DidAddContentSecurityPolicies(
    WTF::Vector<network::mojom::blink::ContentSecurityPolicyPtr> policies) {
  auto* window = DynamicTo<LocalDOMWindow>(execution_context_.Get());
  if (!window)
    return;

  LocalFrame* frame = window->GetFrame();
  if (!frame)
    return;

  // Record what source was used to find main frame CSP. Do not record
  // this for fence frame roots since they will never become an
  // outermost main frame.
  if (frame->IsMainFrame() && !frame->IsInFencedFrameTree()) {
    for (const auto& policy : policies) {
      switch (policy->header->source) {
        case network::mojom::ContentSecurityPolicySource::kHTTP:
          Count(WebFeature::kMainFrameCSPViaHTTP);
          break;
        case network::mojom::ContentSecurityPolicySource::kMeta:
          Count(WebFeature::kMainFrameCSPViaMeta);
          break;
      }
    }
  }
}

SecurityContext& ExecutionContextCSPDelegate::GetSecurityContext() {
  return execution_context_->GetSecurityContext();
}

Document* ExecutionContextCSPDelegate::GetDocument() {
  auto* window = DynamicTo<LocalDOMWindow>(execution_context_.Get());
  return window ? window->document() : nullptr;
}

void ExecutionContextCSPDelegate::DispatchViolationEventInternal(
    const SecurityPolicyViolationEventInit* violation_data,
    Element* element) {
  // Worklets don't support Events in general.
  if (execution_context_->IsWorkletGlobalScope())
    return;

  // https://w3c.github.io/webappsec-csp/#report-violation.
  // Step 3.1. If target is not null, and global is a Window, and target’s
  // shadow-including root is not global’s associated Document, set target to
  // null. [spec text]
  // Step 3.2. If target is null:
  //    Step 3.2.1. Set target be violation’s global object.
  //    Step 3.2.2. If target is a Window, set target to target’s associated
  //    Document. [spec text]
  // Step 3.3. Fire an event named securitypolicyviolation that uses the
  // SecurityPolicyViolationEvent interface at target.. [spec text]
  SecurityPolicyViolationEvent& event = *SecurityPolicyViolationEvent::Create(
      event_type_names::kSecuritypolicyviolation, violation_data);
  DCHECK(event.bubbles());

  if (auto* document = GetDocument()) {
    if (element && element->isConnected() && element->GetDocument() == document)
      element->DispatchEvent(event);
    else
      document->DispatchEvent(event);
  } else if (auto* scope = DynamicTo<WorkerGlobalScope>(*execution_context_)) {
    scope->DispatchEvent(event);
  }
}

}  // namespace blink

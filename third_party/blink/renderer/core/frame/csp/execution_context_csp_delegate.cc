// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/csp/execution_context_csp_delegate.h"

#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/events/security_policy_violation_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/csp/csp_violation_report_body.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/report.h"
#include "third_party/blink/renderer/core/frame/reporting_context.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/ping_loader.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/network/encoded_form_data.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

ExecutionContextCSPDelegate::ExecutionContextCSPDelegate(
    ExecutionContext& execution_context)
    : execution_context_(&execution_context) {}

void ExecutionContextCSPDelegate::Trace(blink::Visitor* visitor) {
  visitor->Trace(execution_context_);
  ContentSecurityPolicyDelegate::Trace(visitor);
}

const SecurityOrigin* ExecutionContextCSPDelegate::GetSecurityOrigin() {
  return execution_context_->GetSecurityOrigin();
}

const KURL& ExecutionContextCSPDelegate::Url() const {
  return execution_context_->Url();
}

void ExecutionContextCSPDelegate::SetSandboxFlags(SandboxFlags mask) {
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
    worklet_or_worker->ApplySandboxFlags(mask);
  }
  // Just check that all the sandbox flags that are set by CSP have
  // already been set on the security context. Meta tags can't set them
  // and we should have already constructed the document with the correct
  // sandbox flags from CSP already.
  WebSandboxFlags flags = GetSecurityContext().GetSandboxFlags();
  CHECK_EQ(flags | mask, flags);
}

void ExecutionContextCSPDelegate::SetRequireTrustedTypes() {
  GetSecurityContext().SetRequireTrustedTypes();
}

void ExecutionContextCSPDelegate::AddInsecureRequestPolicy(
    WebInsecureRequestPolicy policy) {
  SecurityContext& security_context = GetSecurityContext();

  Document* document = GetDocument();

  // Step 2. Set settings’s insecure requests policy to Upgrade. [spec text]
  // Upgrade Insecure Requests: Update the policy.
  security_context.SetInsecureRequestPolicy(
      security_context.GetInsecureRequestPolicy() | policy);
  if (document)
    document->DidEnforceInsecureRequestPolicy();

  // Upgrade Insecure Requests: Update the set of insecure URLs to upgrade.
  if (policy & kUpgradeInsecureRequests) {
    // Spec: Enforcing part of:
    // https://w3c.github.io/webappsec-upgrade-insecure-requests/#delivery
    // Step 3. Let tuple be a tuple of the protected resource’s URL's host and
    // port. [spec text]
    // Step 4. Insert tuple into settings’s upgrade insecure navigations set.
    // [spec text]
    Count(WebFeature::kUpgradeInsecureRequestsEnabled);
    // We don't add the hash if |document| is null, to prevent
    // WorkerGlobalScope::Url() before it's ready. https://crbug.com/861564
    // This should be safe, because the insecure navigations set is not used
    // in non-Document contexts.
    if (document && !Url().Host().IsEmpty()) {
      uint32_t hash = Url().Host().Impl()->GetHash();
      security_context.AddInsecureNavigationUpgrade(hash);
      document->DidEnforceInsecureNavigationsSet();
    }
  }
}

std::unique_ptr<SourceLocation>
ExecutionContextCSPDelegate::GetSourceLocation() {
  return SourceLocation::Capture(execution_context_);
}

base::Optional<uint16_t> ExecutionContextCSPDelegate::GetStatusCode() {
  base::Optional<uint16_t> status_code;

  // TODO(mkwst): We only have status code information for Documents. It would
  // be nice to get them for Workers as well.
  Document* document = GetDocument();
  if (document && !SecurityOrigin::IsSecure(document->Url()) &&
      document->Loader()) {
    status_code = document->Loader()->GetResponse().HttpStatusCode();
  }

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
          WTF::Bind(
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
            ContentSecurityPolicy::DirectiveType::kFrameAncestors ==
                ContentSecurityPolicy::GetDirectiveType(
                    violation_data.effectiveDirective()));

  // TODO(crbug/929370): Support POSTing violation reports from a Worker.
  Document* document = GetDocument();
  if (!document)
    return;

  LocalFrame* frame = document->GetFrame();
  if (!frame)
    return;

  scoped_refptr<EncodedFormData> report =
      EncodedFormData::Create(stringified_report.Utf8());

  // Construct and route the report to the ReportingContext, to be observed
  // by any ReportingObservers.
  auto* body = MakeGarbageCollected<CSPViolationReportBody>(violation_data);
  Report* observed_report = MakeGarbageCollected<Report>(
      ReportType::kCSPViolation, Url().GetString(), body);
  ReportingContext::From(document)->QueueReport(
      observed_report, use_reporting_api ? report_endpoints : Vector<String>());

  if (use_reporting_api)
    return;

  for (const auto& report_endpoint : report_endpoints) {
    // Use the frame's document to complete the endpoint URL, overriding its URL
    // with the blocked document's URL.
    // https://w3c.github.io/webappsec-csp/#report-violation
    // Step 3.4.2.1. Let endpoint be the result of executing the URL parser with
    // token as the input, and violation’s url as the base URL. [spec text]
    KURL url = is_frame_ancestors_violation
                   ? document->CompleteURLWithOverride(
                         report_endpoint, KURL(violation_data.blockedURI()))
                   // We use the FallbackBaseURL to ensure that we don't
                   // respect base elements when determining the report
                   // endpoint URL.
                   // Note: According to Step 3.4.2.1 mentioned above, the base
                   // URL is "violation’s url" which should be violation's
                   // global object's URL. So using FallbackBaseURL() might be
                   // inconsistent.
                   : document->CompleteURLWithOverride(
                         report_endpoint, document->FallbackBaseURL());
    PingLoader::SendViolationReport(frame, url, report);
  }
}

void ExecutionContextCSPDelegate::Count(WebFeature feature) {
  UseCounter::Count(execution_context_, feature);
}

void ExecutionContextCSPDelegate::AddConsoleMessage(
    ConsoleMessage* console_message) {
  execution_context_->AddConsoleMessage(console_message);
}

void ExecutionContextCSPDelegate::DisableEval(const String& error_message) {
  execution_context_->DisableEval(error_message);
}

void ExecutionContextCSPDelegate::ReportBlockedScriptExecutionToInspector(
    const String& directive_text) {
  probe::ScriptExecutionBlockedByCSP(execution_context_, directive_text);
}

void ExecutionContextCSPDelegate::DidAddContentSecurityPolicies(
    const blink::WebVector<WebContentSecurityPolicy>& policies) {
  Document* document = GetDocument();
  if (document && document->GetFrame())
    document->GetFrame()->Client()->DidAddContentSecurityPolicies(policies);
}

SecurityContext& ExecutionContextCSPDelegate::GetSecurityContext() {
  return execution_context_->GetSecurityContext();
}

Document* ExecutionContextCSPDelegate::GetDocument() {
  return DynamicTo<Document>(execution_context_.Get());
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
      element->EnqueueEvent(event, TaskType::kInternalDefault);
    else
      document->EnqueueEvent(event, TaskType::kInternalDefault);
  } else if (auto* scope = DynamicTo<WorkerGlobalScope>(*execution_context_)) {
    scope->EnqueueEvent(event, TaskType::kInternalDefault);
  }
}

}  // namespace blink

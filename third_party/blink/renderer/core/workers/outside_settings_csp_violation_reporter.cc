// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/outside_settings_csp_violation_reporter.h"

#include <utility>

#include "base/task/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/message.h"
#include "services/network/public/mojom/content_security_policy.mojom-blink.h"
#include "services/network/public/mojom/source_location.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/platform/bindings/source_location.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

// static
const char OutsideSettingsCSPViolationReporter::kSupplementName[] =
    "OutsideSettingsCSPViolationReporter";

// static
mojo::PendingRemote<mojom::blink::OutsideContextCSPViolationReporter>
OutsideSettingsCSPViolationReporter::BindNewRemote(ExecutionContext& context) {
  if (!RuntimeEnabledFeatures::
          DispatchWorkerCSPViolationEventForModuleGraphFetchingEnabled()) {
    return mojo::NullRemote();
  }

  auto* reporter =
      Supplement<ExecutionContext>::From<OutsideSettingsCSPViolationReporter>(
          context);
  if (!reporter) {
    reporter =
        MakeGarbageCollected<OutsideSettingsCSPViolationReporter>(context);
    ProvideTo(context, reporter);
  }

  // TODO(crbug.com/40612680): The violation reporting steps queue a task to
  // dispatch the securitypolicyviolation event (see
  // ContentSecurityPolicy::LogToConsoleAndReportViolation() and
  // ExecutionContextCSPDelegate::DispatchViolationEvent()). The worker or the
  // worklet detects the violation before it queues the task notifying the
  // fetch failure (e.g. the rejection of the worklet.addModule() promise or the
  // error event on the worker), so the event is expected to be dispatched
  // first. However, receiving the violation through this interface adds
  // another task before the event dispatching task is queued, so the event can
  // be dispatched after the failure. Need to dispatch the event within the task
  // receiving the violation to keep the order.
  mojo::PendingRemote<mojom::blink::OutsideContextCSPViolationReporter> remote;
  reporter->receivers_.Add(remote.InitWithNewPipeAndPassReceiver(),
                           context.GetTaskRunner(TaskType::kNetworking));
  return remote;
}

OutsideSettingsCSPViolationReporter::OutsideSettingsCSPViolationReporter(
    ExecutionContext& context)
    : Supplement<ExecutionContext>(context), receivers_(this, &context) {}

void OutsideSettingsCSPViolationReporter::ReportCSPViolation(
    network::mojom::blink::CSPViolationPtr violation) {
  // The fetches with the outside settings object are the module graph fetches
  // of workers (worker-src) and worklets (script-src-elem), including the JSON
  // and text module imports in them (connect-src) [1][2], and their mixed
  // content checks (block-all-mixed-content, obsolete in the current spec [3]
  // but still supported).
  //
  // [1] https://w3c.github.io/webappsec-csp/#effective-directive-for-a-request
  // [2] https://html.spec.whatwg.org/#fetch-destination-from-module-type
  // [3] https://www.w3.org/TR/mixed-content/#strict-checking
  CSPDirectiveName effective_type =
      ContentSecurityPolicy::GetDirectiveType(violation->effective_directive);
  if (effective_type != CSPDirectiveName::WorkerSrc &&
      effective_type != CSPDirectiveName::ScriptSrcElem &&
      effective_type != CSPDirectiveName::ConnectSrc &&
      effective_type != CSPDirectiveName::BlockAllMixedContent) {
    mojo::ReportBadMessage("Unexpected CSP violation effective directive");
    return;
  }

  ExecutionContext* context = GetSupplementable();
  if (!context || context->IsContextDestroyed()) {
    return;
  }
  if (ContentSecurityPolicy* policy = context->GetContentSecurityPolicy()) {
    policy->LogToConsoleAndReportViolation(
        effective_type, violation->console_message, violation->blocked_url,
        violation->report_endpoints, violation->use_reporting_api,
        violation->header, violation->type,
        ContentSecurityPolicyViolationType::kURLViolation,
        MakeGarbageCollected<SourceLocation>(
            violation->source_location->url, String(),
            violation->source_location->line,
            violation->source_location->column, nullptr));
  }
}

void OutsideSettingsCSPViolationReporter::Trace(Visitor* visitor) const {
  visitor->Trace(receivers_);
  Supplement<ExecutionContext>::Trace(visitor);
}

}  // namespace blink

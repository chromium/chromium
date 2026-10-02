// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/main_thread_worklet_reporting_proxy.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/events/security_policy_violation_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

MainThreadWorkletReportingProxy::MainThreadWorkletReportingProxy(
    ExecutionContext* context)
    : context_(context) {}

void MainThreadWorkletReportingProxy::CountFeature(WebFeature feature) {
  DCHECK(IsMainThread());
  // A parent context is on the same thread, so just record API use in the
  // context's UseCounter.
  UseCounter::Count(context_, feature);
}

void MainThreadWorkletReportingProxy::CountWebDXFeature(
    mojom::blink::WebDXFeature feature) {
  DCHECK(IsMainThread());
  // A parent context is on the same thread, so just record API use in the
  // context's UseCounter.
  UseCounter::CountWebDXFeature(context_, feature);
}

void MainThreadWorkletReportingProxy::DispatchCSPViolationEvent(
    const SecurityPolicyViolationEventInit& violation_data) {
  DCHECK(IsMainThread());

  // The violation's global object is request's client's global object
  // (CSP 2.4.2). Since the worklet's module graph fetch uses the outside
  // settings Window for the fetch client (HTML 8.1.4.2), the target here
  // is the Window. So the violation event is dispatched on its associated
  // Document as described in the reporting step (CSP 5.5 / step 3.2.2)
  // - https://w3c.github.io/webappsec-csp/#create-violation-for-request
  // - https://html.spec.whatwg.org/#fetch-a-worklet-script-graph
  // - https://html.spec.whatwg.org/#fetch-a-single-module-script
  // - https://w3c.github.io/webappsec-csp/#report-violation
  if (auto* window = DynamicTo<LocalDOMWindow>(context_.Get())) {
    if (auto* document = window->document()) {
      // As DispatchCSPViolationEvent() is called synchronously in the middle
      // of the worklet's module-fetch algorithm, enqueue the violation event
      // so the event is fired asynchronously as per the CSP 5.5 / step 3:
      // "Queue a task to run the following steps:".
      document->EnqueueEvent(
          *SecurityPolicyViolationEvent::Create(
              event_type_names::kSecuritypolicyviolation, &violation_data),
          TaskType::kNetworking);
    }
  }
}

void MainThreadWorkletReportingProxy::DidTerminateWorkerThread() {
  // MainThreadWorklet does not start and terminate a thread.
  NOTREACHED();
}

}  // namespace blink

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/worker_module_tree_client.h"

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/script/module_script.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_reporting_proxy.h"

namespace blink {

WorkerModuleTreeClient::WorkerModuleTreeClient(ScriptState* script_state)
    : script_state_(script_state) {}

// A partial implementation of the "Processing model" algorithm in the HTML
// WebWorker spec:
// https://html.spec.whatwg.org/C/#worker-processing-model
void WorkerModuleTreeClient::NotifyModuleTreeLoadFinished(
    ModuleScript* module_script) {
  auto* worker_global_scope =
      To<WorkerGlobalScope>(ExecutionContext::From(script_state_));
  blink::WorkerReportingProxy& worker_reporting_proxy =
      worker_global_scope->ReportingProxy();

  // Step 12. "If the algorithm asynchronously completes with null, then:"
  if (!module_script) {
    // Step 12.1. "Queue a task to fire an event named error at worker."
    // DidFailToFetchModuleScript() will asynchronously fire the event.
    worker_reporting_proxy.DidFailToFetchModuleScript();

    // Step 12.2. "Run the environment discarding steps for inside settings."
    // Do nothing because the HTML spec doesn't define these steps for web
    // workers.

    // Schedule worker termination.
    worker_global_scope->close();

    // Step 12.3. "Return."
    return;
  }
  worker_reporting_proxy.DidFetchScript();

  // Step 12: "Otherwise, continue the rest of these steps after the algorithm's
  // asynchronous completion, with script being the asynchronous completion
  // value."
  worker_global_scope->WorkerScriptFetchFinished(
      *module_script, base::nullopt /* v8_inspector::V8StackTraceId */);
}

void WorkerModuleTreeClient::Trace(blink::Visitor* visitor) {
  visitor->Trace(script_state_);
  ModuleTreeClient::Trace(visitor);
}

}  // namespace blink

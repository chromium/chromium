// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/worker_module_tree_client.h"

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/events/error_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/script/module_script.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_reporting_proxy.h"

namespace blink {

WorkerModuleTreeClient::WorkerModuleTreeClient(Modulator* modulator)
    : modulator_(modulator) {}

// A partial implementation of the "Processing model" algorithm in the HTML
// WebWorker spec:
// https://html.spec.whatwg.org/multipage/workers.html#worker-processing-model
void WorkerModuleTreeClient::NotifyModuleTreeLoadFinished(
    ModuleScript* module_script) {
  auto* execution_context =
      ExecutionContext::From(modulator_->GetScriptState());
  blink::WorkerReportingProxy& worker_reporting_proxy =
      To<WorkerGlobalScope>(execution_context)->ReportingProxy();

  if (!module_script) {
    // Step 12: "If the algorithm asynchronously completes with null, queue
    // a task to fire an event named error at worker, and return."
    // This ErrorEvent object is just used for passing error information to a
    // worker object on the parent context thread and not dispatched directly.
    execution_context->ExceptionThrown(
        ErrorEvent::Create("Failed to load a module script.",
                           SourceLocation::Capture(), nullptr /* world */));
    return;
  }

  // Step 12: "Otherwise, continue the rest of these steps after the algorithm's
  // asynchronous completion, with script being the asynchronous completion
  // value."
  worker_reporting_proxy.WillEvaluateModuleScript();
  ScriptValue error = modulator_->ExecuteModule(
      module_script, Modulator::CaptureEvalErrorFlag::kReport);
  worker_reporting_proxy.DidEvaluateModuleScript(error.IsEmpty());
}

void WorkerModuleTreeClient::Trace(blink::Visitor* visitor) {
  visitor->Trace(modulator_);
  ModuleTreeClient::Trace(visitor);
}

}  // namespace blink

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/classic_script.h"

#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/script_source_code.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_reporting_proxy.h"

namespace blink {

void ClassicScript::Trace(Visitor* visitor) {
  Script::Trace(visitor);
  visitor->Trace(script_source_code_);
}

void ClassicScript::RunScript(LocalFrame* frame,
                              const SecurityOrigin* security_origin) {
  frame->GetScriptController().ExecuteScriptInMainWorld(
      GetScriptSourceCode(), BaseURL(), sanitize_script_errors_,
      FetchOptions());
}

void ClassicScript::RunScriptOnWorker(WorkerGlobalScope& worker_global_scope) {
  DCHECK(worker_global_scope.IsContextThread());

  WorkerReportingProxy& worker_reporting_proxy =
      worker_global_scope.ReportingProxy();

  worker_reporting_proxy.WillEvaluateClassicScript(
      GetScriptSourceCode().Source().length(),
      GetScriptSourceCode().CacheHandler()
          ? GetScriptSourceCode().CacheHandler()->GetCodeCacheSize()
          : 0);
  bool success = worker_global_scope.ScriptController()->Evaluate(
      GetScriptSourceCode(), sanitize_script_errors_, nullptr /* error_event */,
      worker_global_scope.GetV8CacheOptions());
  worker_reporting_proxy.DidEvaluateClassicScript(success);
}

}  // namespace blink

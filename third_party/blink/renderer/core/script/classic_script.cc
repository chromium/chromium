// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/classic_script.h"

#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/script_source_code.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/workers/worker_or_worklet_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_reporting_proxy.h"

namespace blink {

ClassicScript* ClassicScript::CreateUnspecifiedScript(
    const ScriptSourceCode& script_source_code,
    SanitizeScriptErrors sanitize_script_errors) {
  return MakeGarbageCollected<ClassicScript>(
      script_source_code, KURL(), ScriptFetchOptions(), sanitize_script_errors);
}

void ClassicScript::Trace(Visitor* visitor) const {
  Script::Trace(visitor);
  visitor->Trace(script_source_code_);
}

void ClassicScript::RunScript(LocalDOMWindow* window) {
  return RunScript(window,
                   ScriptController::kDoNotExecuteScriptWhenScriptsDisabled);
}

void ClassicScript::RunScript(LocalDOMWindow* window,
                              ScriptController::ExecuteScriptPolicy policy) {
  v8::HandleScope handle_scope(window->GetIsolate());
  RunScriptAndReturnValue(window, policy);
}

v8::Local<v8::Value> ClassicScript::RunScriptAndReturnValue(
    LocalDOMWindow* window,
    ScriptController::ExecuteScriptPolicy policy) {
  return window->GetScriptController().EvaluateScriptInMainWorld(
      GetScriptSourceCode(), BaseURL(), sanitize_script_errors_, FetchOptions(),
      policy);
}

v8::Local<v8::Value> ClassicScript::RunScriptInIsolatedWorldAndReturnValue(
    LocalDOMWindow* window,
    int32_t world_id) {
  return window->GetScriptController().ExecuteScriptInIsolatedWorld(
      world_id, GetScriptSourceCode(), BaseURL(), sanitize_script_errors_);
}

bool ClassicScript::RunScriptOnWorkerOrWorklet(
    WorkerOrWorkletGlobalScope& global_scope) {
  DCHECK(global_scope.IsContextThread());

  v8::HandleScope handle_scope(
      global_scope.ScriptController()->GetScriptState()->GetIsolate());
  ScriptEvaluationResult result =
      global_scope.ScriptController()->EvaluateAndReturnValue(
          GetScriptSourceCode(), sanitize_script_errors_);
  return result.GetResultType() == ScriptEvaluationResult::ResultType::kSuccess;
}

std::pair<size_t, size_t> ClassicScript::GetClassicScriptSizes() const {
  size_t cached_metadata_size =
      GetScriptSourceCode().CacheHandler()
          ? GetScriptSourceCode().CacheHandler()->GetCodeCacheSize()
          : 0;
  return std::pair<size_t, size_t>(GetScriptSourceCode().Source().length(),
                                   cached_metadata_size);
}

}  // namespace blink

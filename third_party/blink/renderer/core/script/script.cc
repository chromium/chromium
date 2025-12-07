// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/script.h"

#include <optional>

#include "third_party/blink/renderer/bindings/core/v8/script_evaluation_result.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/script_type_names.h"

namespace blink {

mojom::blink::ScriptType Script::V8WorkerTypeToScriptType(
    V8WorkerType::Enum worker_script_type) {
  switch (worker_script_type) {
    case V8WorkerType::Enum::kClassic:
      return mojom::blink::ScriptType::kClassic;
    case V8WorkerType::Enum::kModule:
      return mojom::blink::ScriptType::kModule;
  }
  NOTREACHED();
}

void Script::RunScriptOnScriptState(
    ScriptState* script_state,
    ExecuteScriptPolicy execute_script_policy,
    V8ScriptRunner::RethrowErrorsOption rethrow_errors) {
  if (!script_state)
    return;

  v8::HandleScope scope(script_state->GetIsolate());
  std::ignore = RunScriptOnScriptStateAndReturnValue(
      script_state, execute_script_policy, std::move(rethrow_errors));
}

void Script::RunScript(LocalDOMWindow* window,
                       ExecuteScriptPolicy execute_script_policy,
                       V8ScriptRunner::RethrowErrorsOption rethrow_errors) {
  RunScriptOnScriptState(ToScriptStateForMainWorld(window->GetFrame()),
                         execute_script_policy, std::move(rethrow_errors));
}

ScriptEvaluationResult Script::RunScriptAndReturnValue(
    LocalDOMWindow* window,
    ExecuteScriptPolicy execute_script_policy,
    V8ScriptRunner::RethrowErrorsOption rethrow_errors) {
  return RunScriptOnScriptStateAndReturnValue(
      ToScriptStateForMainWorld(window->GetFrame()), execute_script_policy,
      std::move(rethrow_errors));
}

}  // namespace blink

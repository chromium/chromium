// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scheduler/script_wrappable_task_state.h"

#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/core/scheduler/script_wrappable_task_state.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "v8/include/v8.h"

namespace blink {

ScriptWrappableTaskState::ScriptWrappableTaskState(
    WrappableTaskState* task_state)
    : wrapped_task_state_(task_state) {
  CHECK(wrapped_task_state_);
}

void ScriptWrappableTaskState::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(wrapped_task_state_);
}

// static
ScriptWrappableTaskState* ScriptWrappableTaskState::GetCurrent(
    v8::Isolate* isolate) {
  CHECK(isolate);
  if (isolate->IsExecutionTerminating()) {
    return nullptr;
  }
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Value> v8_value =
      isolate->GetContinuationPreservedEmbedderData();
  if (v8_value->IsNullOrUndefined()) {
    return nullptr;
  }
  // If not empty, the value must be a `ScriptWrappableTaskState`.
  NonThrowableExceptionState exception_state;
  ScriptWrappableTaskState* task_state =
      NativeValueTraits<ScriptWrappableTaskState>::NativeValue(
          isolate, v8_value, exception_state);
  DCHECK(task_state);
  return task_state;
}

// static
void ScriptWrappableTaskState::SetCurrent(
    ScriptState* script_state,
    ScriptWrappableTaskState* task_state) {
  DCHECK(script_state);
  v8::Isolate* isolate = script_state->GetIsolate();
  DCHECK(isolate);
  if (isolate->IsExecutionTerminating()) {
    return;
  }
  CHECK(!ScriptForbiddenScope::IsScriptForbidden());
  // `task_state` will be null when leaving the top-level task scope, at which
  // point we want to clear the isolate's CPED and reference to the related
  // context. We don't need to distinguish between null and undefined values,
  // and V8 has a fast path if the CPED is undefined, so treat null `task_state`
  // as undefined.
  //
  // TODO(crbug.com/1351643): Since the context no longer matters, change this
  // to a utility context that will always be valid.
  if (!script_state->ContextIsValid() || !task_state) {
    isolate->SetContinuationPreservedEmbedderData(v8::Undefined(isolate));
  } else {
    ScriptState::Scope scope(script_state);
    isolate->SetContinuationPreservedEmbedderData(
        ToV8Traits<ScriptWrappableTaskState>::ToV8(script_state, task_state));
  }
}

}  // namespace blink

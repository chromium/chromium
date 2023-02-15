// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/scheduler/script_wrappable_task_state.h"

#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/modules/scheduler/script_wrappable_task_state.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

ScriptWrappableTaskState::ScriptWrappableTaskState(
    scheduler::TaskAttributionId id)
    : task_attribution_id_(id) {}

// static
ScriptWrappableTaskState* ScriptWrappableTaskState::GetCurrent(
    ScriptState* script_state) {
  DCHECK(script_state);
  if (!script_state->ContextIsValid()) {
    return nullptr;
  }

  ScriptState::Scope scope(script_state);
  v8::Local<v8::Context> context = script_state->GetContext();
  DCHECK(!context.IsEmpty());
  v8::Local<v8::Value> v8_value =
      context->GetContinuationPreservedEmbedderData();
  if (v8_value->IsNullOrUndefined()) {
    return nullptr;
  }
  v8::Isolate* isolate = script_state->GetIsolate();
  DCHECK(isolate);
  if (isolate->IsExecutionTerminating()) {
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
  if (!script_state->ContextIsValid()) {
    return;
  }
  CHECK(!ScriptForbiddenScope::IsScriptForbidden());
  ScriptState::Scope scope(script_state);
  v8::Isolate* isolate = script_state->GetIsolate();
  DCHECK(isolate);
  if (isolate->IsExecutionTerminating()) {
    return;
  }
  v8::Local<v8::Context> context = script_state->GetContext();
  DCHECK(!context.IsEmpty());

  if (task_state) {
    context->SetContinuationPreservedEmbedderData(
        ToV8Traits<ScriptWrappableTaskState>::ToV8(script_state, task_state)
            .ToLocalChecked());
  } else {
    context->SetContinuationPreservedEmbedderData(v8::Local<v8::Value>());
  }
}

}  // namespace blink

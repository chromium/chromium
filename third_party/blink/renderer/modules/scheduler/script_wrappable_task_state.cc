// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/scheduler/script_wrappable_task_state.h"

#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/modules/scheduler/dom_task_signal.h"
#include "third_party/blink/renderer/modules/scheduler/script_wrappable_task_state.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"

namespace blink {

namespace {

void ClearContinuationPreservedEmbedderData(v8::Isolate* isolate) {
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context =
      V8PerIsolateData::From(isolate)->EnsureScriptRegexpContext();
  v8::Context::Scope context_scope(context);
  context->SetContinuationPreservedEmbedderData(v8::Local<v8::Value>());
}

}  // namespace

ScriptWrappableTaskState::ScriptWrappableTaskState(
    scheduler::TaskAttributionInfo* task,
    AbortSignal* abort_source,
    DOMTaskSignal* priority_source)
    : task_(task),
      abort_source_(abort_source),
      priority_source_(priority_source) {}

void ScriptWrappableTaskState::Trace(Visitor* visitor) const {
  visitor->Trace(abort_source_);
  visitor->Trace(priority_source_);
  visitor->Trace(task_);
  ScriptWrappable::Trace(visitor);
}

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
  v8::Isolate* isolate = script_state->GetIsolate();
  DCHECK(isolate);
  if (isolate->IsExecutionTerminating()) {
    return;
  }
  CHECK(!ScriptForbiddenScope::IsScriptForbidden());
  if (!script_state->ContextIsValid()) {
    // TODO(crbug.com/1351643): This is a temporary workaround for detached
    // contexts while transitioning to per-isolate CPED. When v8 switches to
    // per-isolate CPED, we won't restore the previous value if the context is
    // detached, which can result in propagating the wrong value or leaking the
    // context.
    //
    // The following prevents this by clearing the CPED on an arbitrary context
    // associated with the isolate. Before the v8 API changes, this is a no-op
    // (aside from potentially creating the context). After it changes, this
    // will clear the per-isolate CPED since
    // Context::SetContinuationPreservedEmbedderData() will delegate to
    // Isolate::SetContinuationPreservedEmbedderData().
    ClearContinuationPreservedEmbedderData(isolate);
    return;
  }
  ScriptState::Scope scope(script_state);
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

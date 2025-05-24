// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scheduler/script_wrappable_task_state.h"

#include "base/feature_list.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/core/scheduler/script_wrappable_task_state.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/wrapper_type_info.h"
#include "v8/include/v8-cpp-heap-external.h"
#include "v8/include/v8.h"

namespace blink {

namespace {
constexpr v8::CppHeapPointerTag kWrappableTaskStateTag =
    static_cast<v8::CppHeapPointerTag>(
        CppHeapExternalTag::kWrappableTaskStateTag);
constexpr v8::CppHeapPointerTagRange kWrappableTaskStateTagRange(
    kWrappableTaskStateTag,
    kWrappableTaskStateTag);
}  // namespace

BASE_FEATURE(kTaskAttributionUsesV8CppHeapExternal,
             "TaskAttributionUsesV8CppHeapExternal",
             base::FEATURE_ENABLED_BY_DEFAULT);

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
ScriptWrappableTaskStateBase* ScriptWrappableTaskState::GetCurrent(
    v8::Isolate* isolate) {
  CHECK(isolate);
  if (isolate->IsExecutionTerminating()) {
    return nullptr;
  }
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Data> v8_data =
      isolate->GetContinuationPreservedEmbedderDataV2();
  if (base::FeatureList::IsEnabled(kTaskAttributionUsesV8CppHeapExternal)) {
    if (v8_data->IsValue()) {
      DCHECK(v8::Value::Cast(*v8_data)->IsNullOrUndefined());
      return nullptr;
    }
    DCHECK(v8_data->IsCppHeapExternal());
    return v8::CppHeapExternal::Cast(*v8_data)->Value<WrappableTaskState>(
        isolate, kWrappableTaskStateTagRange);
  } else {
    DCHECK(v8_data->IsValue());
    auto v8_value = v8_data.As<v8::Value>();
    if (v8_value->IsNullOrUndefined()) {
      return nullptr;
    }
    // If not empty, the value must be a `ScriptWrappableTaskState`.
    NonThrowableExceptionState exception_state;
    auto* task_state = NativeValueTraits<ScriptWrappableTaskState>::NativeValue(
        isolate, v8_value, exception_state);
    CHECK(task_state);
    return task_state;
  }
}

// static
void ScriptWrappableTaskState::SetCurrent(
    ScriptState* script_state,
    ScriptWrappableTaskStateBase* task_state) {
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
  // TODO(crbug.com/376599402): `script_state` won't be needed after
  // `kTaskAttributionUsesV8CppHeapExternal` is removed.
  if (!script_state->ContextIsValid() || !task_state) {
    isolate->SetContinuationPreservedEmbedderDataV2(v8::Undefined(isolate));
  } else if (base::FeatureList::IsEnabled(
                 kTaskAttributionUsesV8CppHeapExternal)) {
    v8::HandleScope handle_scope(isolate);
    isolate->SetContinuationPreservedEmbedderDataV2(
        v8::CppHeapExternal::New<WrappableTaskState>(
            isolate, To<WrappableTaskState>(task_state),
            kWrappableTaskStateTag));
  } else {
    // If `task_state` is a `ScriptWrappableTaskState`, then it's already
    // wrapped. Otherwise it's a `WrappableTaskState` and needs to be wrapped.
    if (auto* as_wrappable_task_state =
            DynamicTo<WrappableTaskState>(task_state)) {
      task_state = MakeGarbageCollected<ScriptWrappableTaskState>(
          as_wrappable_task_state);
    }
    ScriptState::Scope scope(script_state);
    isolate->SetContinuationPreservedEmbedderDataV2(
        ToV8Traits<ScriptWrappableTaskState>::ToV8(
            script_state, To<ScriptWrappableTaskState>(task_state)));
  }
}

}  // namespace blink

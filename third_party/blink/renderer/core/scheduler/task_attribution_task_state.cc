// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scheduler/task_attribution_task_state.h"

#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/bindings/wrapper_type_info.h"
#include "v8/include/v8-cpp-heap-external.h"
#include "v8/include/v8.h"

namespace blink {

namespace {
constexpr v8::CppHeapPointerTag kTaskAttributionTaskStateTag =
    static_cast<v8::CppHeapPointerTag>(
        CppHeapExternalTag::kTaskAttributionTaskStateTag);
constexpr v8::CppHeapPointerTagRange kTaskAttributionTaskStateTagRange(
    kTaskAttributionTaskStateTag,
    kTaskAttributionTaskStateTag);
}  // namespace

// static
TaskAttributionTaskState* TaskAttributionTaskState::GetCurrent(
    v8::Isolate* isolate) {
  CHECK(isolate);
  if (isolate->IsExecutionTerminating()) {
    return nullptr;
  }
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Data> v8_data =
      isolate->GetContinuationPreservedEmbedderDataV2();
  if (v8_data->IsValue()) {
    DCHECK(v8::Value::Cast(*v8_data)->IsNullOrUndefined());
    return nullptr;
  }
  DCHECK(v8_data->IsCppHeapExternal());
  return v8::CppHeapExternal::Cast(*v8_data)->Value<TaskAttributionTaskState>(
      isolate, kTaskAttributionTaskStateTagRange);
}

// static
void TaskAttributionTaskState::SetCurrent(
    v8::Isolate* isolate,
    TaskAttributionTaskState* task_state) {
  CHECK(isolate);
  if (isolate->IsExecutionTerminating()) {
    return;
  }
  CHECK(!ScriptForbiddenScope::IsScriptForbidden());
  // `task_state` will be null when leaving the top-level task scope, at which
  // point we want to clear the isolate's CPED and reference to the related
  // context. We don't need to distinguish between null and undefined values,
  // and V8 has a fast path if the CPED is undefined, so treat null `task_state`
  // as undefined.
  if (!task_state) {
    isolate->SetContinuationPreservedEmbedderDataV2(v8::Undefined(isolate));
  } else {
    v8::HandleScope handle_scope(isolate);
    isolate->SetContinuationPreservedEmbedderDataV2(
        v8::CppHeapExternal::New<TaskAttributionTaskState>(
            isolate, task_state, kTaskAttributionTaskStateTag));
  }
}

}  // namespace blink

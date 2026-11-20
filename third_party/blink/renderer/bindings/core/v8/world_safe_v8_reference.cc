// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/world_safe_v8_reference.h"

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/unpacked_serialized_script_value.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/wtf/stack_util.h"

namespace blink {

namespace {

bool HasValidExecutionContext(ScriptState* script_state) {
  if (!script_state || !script_state->ContextIsValid()) {
    return false;
  }
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  return execution_context && !execution_context->IsContextDestroyed();
}

}  // namespace

// static
v8::Local<v8::Value> WorldSafeV8ReferenceInternal::ToWorldSafeValue(
    ScriptState* target_script_state,
    const TraceWrapperV8Reference<v8::Value>& v8_reference,
    ScriptState* source_script_state) {
  DCHECK(!v8_reference.IsEmpty());

  v8::Isolate* isolate = target_script_state->GetIsolate();
  v8::Local<v8::Value> value = v8_reference.Get(isolate);
  if (source_script_state &&
      &source_script_state->World() == &target_script_state->World()) {
    return value;
  }

  // Primitives (including references created outside of any V8 context where
  // `source_script_state` is null) are already safe to access across worlds.
  if (!value->IsObject()) {
    return value;
  }

  CHECK(source_script_state);
  if (!HasValidExecutionContext(source_script_state)) {
    return v8::Null(isolate);
  }

  // Serialize in `source_script_state` (including incumbent context) so that
  // property getters, proxy traps, Error stack formatting, and bound native
  // WebIDL callbacks execute strictly in the source realm rather than
  // `target_script_state`.
  scoped_refptr<SerializedScriptValue> serialized;
  {
    ScriptState::Scope source_context_scope(source_script_state);
    v8::Context::BackupIncumbentScope backup_incumbent_scope(
        source_script_state->GetContext());
    serialized =
        SerializedScriptValue::SerializeAndSwallowExceptions(isolate, value);
  }
  CHECK(serialized);
  if (!HasValidExecutionContext(target_script_state)) {
    return v8::Null(isolate);
  }

  if ((!serialized->SharedArrayBuffersContents().empty() ||
       !serialized->SharedImmutableArrayBuffersContents().empty() ||
       serialized->MaybeGetSharedValueConveyor()) &&
      !ExecutionContext::From(target_script_state)
           ->SharedArrayBufferTransferAllowed()) {
    return v8::Null(isolate);
  }

  v8::Context::Scope target_context_scope(target_script_state->GetContext());
  return SerializedScriptValue::Unpack(std::move(serialized))
      ->Deserialize(isolate);
}

// static
void WorldSafeV8ReferenceInternal::MaybeCheckCreationContext(
    v8::Isolate* isolate,
    const v8::Local<v8::Context> current_context,
    const DOMWrapperWorld& current_world,
    const v8::Local<v8::Value> value) {
  if (!value->IsObject()) {
    return;
  }

  // Fast bailout: If we are on the main thread and only a single world exists,
  // we know that all contexts belong to this particular world.
  if (!MayNotBeMainThread() &&
      !DOMWrapperWorld::NonMainWorldsExistInMainThread()) {
    return;
  }

  v8::Local<v8::Context> creation_context;
  // Creation context is null if the value is a remote object.
  if (!value.As<v8::Object>()->GetCreationContext(isolate).ToLocal(
          &creation_context)) {
    return;
  }
  // Early bailout in case contexts are equal.
  if (current_context == creation_context) [[likely]] {
    return;
  }
  // For different contexts we need to check the corresponding worlds.
  CHECK_EQ(&current_world,
           &ScriptState::From(isolate, creation_context)->World());
}

}  // namespace blink

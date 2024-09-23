// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/world_safe_v8_reference.h"

#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

// static
v8::Local<v8::Value> WorldSafeV8ReferenceInternal::ToWorldSafeValue(
    ScriptState* target_script_state,
    const TraceWrapperV8Reference<v8::Value>& v8_reference,
    const DOMWrapperWorld& v8_reference_world) {
  DCHECK(!v8_reference.IsEmpty());

  v8::Isolate* isolate = target_script_state->GetIsolate();

  if (&v8_reference_world == &target_script_state->World())
    return v8_reference.Get(isolate);

  // If |v8_reference| is a v8::Object, clones |v8_reference| in the context of
  // |target_script_state| and returns it.  Otherwise returns |v8_reference|
  // itself that is already safe to access in |target_script_state|.

  v8::Local<v8::Value> value = v8_reference.Get(isolate);
  if (!value->IsObject())
    return value;

  v8::Context::Scope target_context_scope(target_script_state->GetContext());
  return SerializedScriptValue::SerializeAndSwallowExceptions(isolate, value)
      ->Deserialize(isolate);
}

// static
void WorldSafeV8ReferenceInternal::MaybeCheckCreationContextWorld(
    const DOMWrapperWorld& world,
    v8::Local<v8::Value> value) {
  if (!value->IsObject())
    return;

  v8::Local<v8::Context> context;
  // Creation context is null if the value is a remote object.
  if (!value.As<v8::Object>()->GetCreationContext().ToLocal(&context))
    return;

  v8::Isolate* isolate = context->GetIsolate();
  ScriptState* script_state = ScriptState::From(isolate, context);
  CHECK_EQ(&world, &script_state->World());
}

}  // namespace blink

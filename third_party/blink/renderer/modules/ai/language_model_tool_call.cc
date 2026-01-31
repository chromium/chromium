// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/language_model_tool_call.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_tool_call_init.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

LanguageModelToolCall* LanguageModelToolCall::Create(
    LanguageModelToolCallInit* init,
    ExceptionState& exception_state) {
  if (!init->hasCallID() || !init->hasName()) {
    exception_state.ThrowTypeError(
        "LanguageModelToolCall requires callID and name.");
    return nullptr;
  }

  ScriptValue arguments;
  if (init->hasArguments()) {
    arguments = init->arguments();
  }

  return MakeGarbageCollected<LanguageModelToolCall>(init->callID(),
                                                     init->name(), arguments);
}

LanguageModelToolCall::LanguageModelToolCall(const String& call_id,
                                             const String& name,
                                             ScriptValue arguments)
    : call_id_(call_id), name_(name), arguments_(arguments) {}

v8::Local<v8::Value> LanguageModelToolCall::arguments(
    ScriptState* script_state) const {
  if (arguments_.IsEmpty()) {
    return v8::Null(script_state->GetIsolate());
  }
  return arguments_.V8Value();
}

void LanguageModelToolCall::Trace(Visitor* visitor) const {
  visitor->Trace(arguments_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink

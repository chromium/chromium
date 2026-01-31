// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/language_model_tool_error.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_tool_error_init.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

LanguageModelToolError* LanguageModelToolError::Create(
    LanguageModelToolErrorInit* init,
    ExceptionState& exception_state) {
  if (!init->hasCallID() || !init->hasName() || !init->hasErrorMessage()) {
    exception_state.ThrowTypeError(
        "LanguageModelToolError requires callID, name, and errorMessage.");
    return nullptr;
  }

  return MakeGarbageCollected<LanguageModelToolError>(
      init->callID(), init->name(), init->errorMessage());
}

LanguageModelToolError::LanguageModelToolError(const String& call_id,
                                               const String& name,
                                               const String& error_message)
    : call_id_(call_id), name_(name), error_message_(error_message) {}

void LanguageModelToolError::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink

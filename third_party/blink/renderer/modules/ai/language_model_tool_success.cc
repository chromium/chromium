// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/language_model_tool_success.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_tool_result_content.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_tool_success_init.h"
#include "third_party/blink/renderer/modules/ai/language_model_create_options.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

LanguageModelToolSuccess* LanguageModelToolSuccess::Create(
    LanguageModelToolSuccessInit* init,
    ExceptionState& exception_state) {
  if (!init->hasCallID() || !init->hasName() || !init->hasResult()) {
    exception_state.ThrowTypeError(
        "LanguageModelToolSuccess requires callID, name, and result.");
    return nullptr;
  }

  return MakeGarbageCollected<LanguageModelToolSuccess>(
      init->callID(), init->name(), init->result());
}

LanguageModelToolSuccess::LanguageModelToolSuccess(
    const String& call_id,
    const String& name,
    const HeapVector<Member<LanguageModelToolResultContent>>& result)
    : call_id_(call_id), name_(name), result_(result) {}

void LanguageModelToolSuccess::Trace(Visitor* visitor) const {
  visitor->Trace(result_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink

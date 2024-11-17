// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/on_device_translation/ai_translator_factory.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/modules/ai/ai.h"

namespace blink {

ScriptPromise<AITranslator> AITranslatorFactory::create(
    ScriptState* script_state,
    AITranslatorCreateOptions* options,
    ExceptionState& exception_state) {
  // TODO(crbug.com/322229993):Take `options` into account.
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The execution context is not valid.");
    return ScriptPromise<AITranslator>();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<AITranslator>>(script_state);
  resolver->Resolve(MakeGarbageCollected<AITranslator>(
      ExecutionContext::From(script_state)
          ->GetTaskRunner(TaskType::kInternalDefault)));
  return resolver->Promise();
}

}  // namespace blink

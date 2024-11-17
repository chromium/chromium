// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/on_device_translation/ai_translator.h"

#include "third_party/blink/public/mojom/on_device_translation/translator.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

AITranslator::AITranslator(scoped_refptr<base::SequencedTaskRunner> task_runner)
    : task_runner_(task_runner) {}

void AITranslator::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(translator_remote_);
}

mojo::PendingReceiver<blink::mojom::blink::Translator>
AITranslator::GetTranslatorReceiver() {
  return translator_remote_.BindNewPipeAndPassReceiver(task_runner_);
}

ScriptPromise<IDLString> AITranslator::translate(
    ScriptState* script_state,
    const WTF::String& input,
    AITranslatorTranslateOptions* options,
    ExceptionState& exception_state) {
  // TODO(crbug.com/322229993): Take `options` into account.
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The execution context is not valid.");
    return EmptyPromise();
  }

  ScriptPromiseResolver<IDLString>* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLString>>(script_state);
  ScriptPromise<IDLString> promise = resolver->Promise();

  // TODO(crbug.com/335374928): implement the error handling for the translation
  // service crash.
  translator_remote_->Translate(
      input, WTF::BindOnce(
                 [](ScriptPromiseResolver<IDLString>* resolver,
                    const WTF::String& output) {
                   if (output.IsNull()) {
                     resolver->Reject(DOMException::Create(
                         "Unable to translate the given text.",
                         DOMException::GetErrorName(
                             DOMExceptionCode::kNotReadableError)));
                   } else {
                     resolver->Resolve(output);
                   }
                 },
                 WrapPersistent(resolver)));

  return promise;
}

void AITranslator::destroy(ScriptState*) {
  // TODO(crbug.com/322229993): Implement the function.
}

}  // namespace blink

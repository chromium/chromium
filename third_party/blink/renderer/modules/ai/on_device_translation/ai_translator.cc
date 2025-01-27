// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/on_device_translation/ai_translator.h"

#include "third_party/blink/public/mojom/on_device_translation/translator.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/ai/exception_helpers.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {
namespace {

const char kExceptionMessageTranslatorDestroyed[] =
    "The translator has been destroyed.";

}  // namespace

AITranslator::AITranslator(
    mojo::PendingRemote<mojom::blink::Translator> pending_remote,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    String source_language,
    String target_language)
    : source_language_(std::move(source_language)),
      target_language_(std::move(target_language)) {
  translator_remote_.Bind(std::move(pending_remote), task_runner);
}

void AITranslator::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(translator_remote_);
}

String AITranslator::sourceLanguage() const {
  return source_language_;
}
String AITranslator::targetLanguage() const {
  return target_language_;
}

ScriptPromise<IDLString> AITranslator::translate(
    ScriptState* script_state,
    const WTF::String& input,
    AITranslatorTranslateOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return EmptyPromise();
  }

  CHECK(options);
  ScriptPromiseResolver<IDLString>* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLString>>(script_state);
  ScriptPromise<IDLString> promise = resolver->Promise();

  AbortSignal* signal = options->getSignalOr(nullptr);
  if (signal && signal->aborted()) {
    resolver->Reject(signal->reason(script_state));
    return promise;
  }

  if (!translator_remote_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kExceptionMessageTranslatorDestroyed);
    return EmptyPromise();
  }

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
  translator_remote_.reset();
}

}  // namespace blink

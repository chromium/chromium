// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/on_device_translation/language_translator.h"

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/public/mojom/on_device_translation/translator.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

LanguageTranslator::LanguageTranslator(
    const String source_lang,
    const String target_lang,
    mojo::PendingRemote<mojom::blink::Translator> pending_remote,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : source_lang_(source_lang), target_lang_(target_lang) {
  translator_remote_.Bind(std::move(pending_remote), task_runner);
}

void LanguageTranslator::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(translator_remote_);
  visitor->Trace(pending_resolvers_);
}

// TODO(crbug.com/322229993): The new version is AITranslator::translate().
// Delete this old version.
ScriptPromise<IDLString> LanguageTranslator::translate(
    ScriptState* script_state,
    const String& input,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The execution context is not valid.");
    return EmptyPromise();
  }

  if (!translator_remote_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The translator has been destoried.");
    return EmptyPromise();
  }

  ScriptPromiseResolver<IDLString>* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLString>>(script_state);
  pending_resolvers_.insert(resolver);
  ScriptPromise<IDLString> promise = resolver->Promise();

  translator_remote_->Translate(
      input, WTF::BindOnce(&LanguageTranslator::OnTranslateFinished,
                           WrapWeakPersistent(this), WrapPersistent(resolver)));
  return promise;
}

void LanguageTranslator::destroy() {
  translator_remote_.reset();
  auto resolvers = std::move(pending_resolvers_);
  for (auto resolver : resolvers) {
    resolver->Reject(DOMException::Create(
        "The translator has been destoried.",
        DOMException::GetErrorName(DOMExceptionCode::kAbortError)));
  }
}

void LanguageTranslator::OnTranslateFinished(
    ScriptPromiseResolver<IDLString>* resolver,
    const WTF::String& output) {
  auto it = pending_resolvers_.find(resolver);
  if (it == pending_resolvers_.end()) {
    return;
  }
  pending_resolvers_.erase(it);

  if (output.IsNull()) {
    resolver->Reject(DOMException::Create(
        "Unable to translate the given text.",
        DOMException::GetErrorName(DOMExceptionCode::kNotReadableError)));
  } else {
    resolver->Resolve(output);
  }
}

}  // namespace blink

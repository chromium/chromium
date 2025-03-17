// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/on_device_translation/ai_translator.h"

#include "base/functional/callback_helpers.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom-blink.h"
#include "third_party/blink/public/mojom/on_device_translation/translator.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/modules/ai/exception_helpers.h"
#include "third_party/blink/renderer/modules/ai/model_execution_responder.h"
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
    : task_runner_(std::move(task_runner)),
      source_language_(std::move(source_language)),
      target_language_(std::move(target_language)) {
  translator_remote_.Bind(std::move(pending_remote), task_runner_);
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

  AbortSignal* signal = options->getSignalOr(nullptr);
  if (HandleAbortSignal(signal, script_state, exception_state)) {
    return EmptyPromise();
  }

  if (!translator_remote_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kExceptionMessageTranslatorDestroyed);
    return EmptyPromise();
  }

  CHECK(options);
  ScriptPromiseResolver<IDLString>* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLString>>(script_state);
  ScriptPromise<IDLString> promise = resolver->Promise();

  auto pending_remote = CreateModelExecutionResponder(
      script_state, signal, resolver, task_runner_,
      AIMetrics::AISessionType::kTranslator,
      /*complete_callback=*/base::DoNothing(),
      /*overflow_callback=*/base::DoNothing());

  // TODO(crbug.com/335374928): implement the error handling for the translation
  // service crash.
  translator_remote_->Translate(input, std::move(pending_remote));

  return promise;
}

ReadableStream* AITranslator::translateStreaming(
    ScriptState* script_state,
    const WTF::String& input,
    AITranslatorTranslateOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return nullptr;
  }

  CHECK(options);
  AbortSignal* signal = options->getSignalOr(nullptr);
  if (HandleAbortSignal(signal, script_state, exception_state)) {
    return nullptr;
  }

  if (!translator_remote_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kExceptionMessageTranslatorDestroyed);
    return nullptr;
  }

  auto [readable_stream, pending_remote] =
      CreateModelExecutionStreamingResponder(
          script_state, signal, task_runner_,
          AIMetrics::AISessionType::kTranslator,
          /*complete_callback=*/base::DoNothing(),
          /*overflow_callback=*/base::DoNothing());

  // TODO(crbug.com/335374928): Implement the error handling for the translation
  // service crash.
  translator_remote_->Translate(input, std::move(pending_remote));

  return readable_stream;
}

void AITranslator::destroy(ScriptState*) {
  translator_remote_.reset();
}

}  // namespace blink

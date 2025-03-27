// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/on_device_translation/translator.h"

#include <limits>

#include "base/functional/callback_helpers.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom-blink.h"
#include "third_party/blink/public/mojom/on_device_translation/translator.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/modules/ai/ai_interface_proxy.h"
#include "third_party/blink/renderer/modules/ai/exception_helpers.h"
#include "third_party/blink/renderer/modules/ai/model_execution_responder.h"
#include "third_party/blink/renderer/modules/ai/on_device_translation/create_translator_client.h"
#include "third_party/blink/renderer/modules/ai/on_device_translation/resolver_with_abort_signal.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {
namespace {
using mojom::blink::CanCreateTranslatorResult;

const char kExceptionMessageTranslatorDestroyed[] =
    "The translator has been destroyed.";

}  // namespace

Translator::Translator(
    mojo::PendingRemote<mojom::blink::Translator> pending_remote,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    String source_language,
    String target_language)
    : task_runner_(std::move(task_runner)),
      source_language_(std::move(source_language)),
      target_language_(std::move(target_language)) {
  translator_remote_.Bind(std::move(pending_remote), task_runner_);
}

void Translator::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(translator_remote_);
}

String Translator::sourceLanguage() const {
  return source_language_;
}
String Translator::targetLanguage() const {
  return target_language_;
}

ScriptPromise<V8AIAvailability> Translator::availability(
    ScriptState* script_state,
    TranslatorCreateCoreOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return ScriptPromise<V8AIAvailability>();
  }

  ScriptPromiseResolver<V8AIAvailability>* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<V8AIAvailability>>(
          script_state);
  ScriptPromise<V8AIAvailability> promise = resolver->Promise();
  ExecutionContext* execution_context = ExecutionContext::From(script_state);

  AIInterfaceProxy::GetTranslationManagerRemote(execution_context)
      ->TranslationAvailable(
          mojom::blink::TranslatorLanguageCode::New(options->sourceLanguage()),
          mojom::blink::TranslatorLanguageCode::New(options->targetLanguage()),
          WTF::BindOnce(
              [](ExecutionContext* execution_context,
                 ScriptPromiseResolver<V8AIAvailability>* resolver,
                 CanCreateTranslatorResult result) {
                CHECK(resolver);

                AIAvailability availability =
                    HandleTranslatorAvailabilityCheckResult(execution_context,
                                                            result);
                resolver->Resolve(AIAvailabilityToV8(availability));
              },
              WrapPersistent(execution_context), WrapPersistent(resolver)));

  return promise;
}

ScriptPromise<Translator> Translator::create(ScriptState* script_state,
                                             TranslatorCreateOptions* options,
                                             ExceptionState& exception_state) {
  // If `sourceLanguage` and `targetLanguage` are not passed, A TypeError should
  // be thrown before we get here.
  CHECK(options && options->sourceLanguage() && options->targetLanguage());

  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The execution context is not valid.");
    return EmptyPromise();
  }

  AbortSignal* signal = options->getSignalOr(nullptr);
  if (HandleAbortSignal(signal, script_state, exception_state)) {
    return EmptyPromise();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<Translator>>(script_state);

  CreateTranslatorClient* create_translator_client =
      MakeGarbageCollected<CreateTranslatorClient>(script_state, options,
                                                   resolver);

  AIInterfaceProxy::GetTranslationManagerRemote(
      ExecutionContext::From(script_state))
      ->CanCreateTranslator(
          mojom::blink::TranslatorLanguageCode::New(options->sourceLanguage()),
          mojom::blink::TranslatorLanguageCode::New(options->targetLanguage()),
          WTF::BindOnce(&CreateTranslatorClient::OnGotAvailability,
                        WrapPersistent(create_translator_client)));

  return resolver->Promise();
}

ScriptPromise<IDLString> Translator::translate(
    ScriptState* script_state,
    const WTF::String& input,
    TranslatorTranslateOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return EmptyPromise();
  }

  // TODO(crbug.com/399693771): This should be a composite signal of the passed
  // in abort signal and the create abort signal.
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

ReadableStream* Translator::translateStreaming(
    ScriptState* script_state,
    const WTF::String& input,
    TranslatorTranslateOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return nullptr;
  }

  // TODO(crbug.com/399693771): This should be a composite signal of the passed
  // in abort signal and the create abort signal.
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

void Translator::destroy(ScriptState*) {
  translator_remote_.reset();
}

ScriptPromise<IDLDouble> Translator::measureInputUsage(
    ScriptState* script_state,
    const WTF::String& input,
    TranslatorTranslateOptions* options,
    ExceptionState& exception_state) {
  // https://webmachinelearning.github.io/writing-assistance-apis/#measure-ai-model-input-usage
  //
  // If modelObject’s relevant global object is a Window whose associated
  // Document is not fully active, then return a promise rejected with an
  // "InvalidStateError" DOMException.
  auto* context = ExecutionContext::From(script_state);
  if (auto* window = DynamicTo<LocalDOMWindow>(context)) {
    auto* document = window->document();
    if (document && !document->IsActive()) {
      exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                        "The document is not active");
      return EmptyPromise();
    }
  }

  // TODO(crbug.com/399693771): This should be a composite signal of the passed
  // in abort signal and the create abort signal.
  CHECK(options);
  AbortSignal* signal = options->getSignalOr(nullptr);
  if (HandleAbortSignal(signal, script_state, exception_state)) {
    return EmptyPromise();
  }

  ResolverWithAbortSignal<IDLDouble>* resolver =
      MakeGarbageCollected<ResolverWithAbortSignal<IDLDouble>>(script_state,
                                                               signal);

  task_runner_->PostTask(
      FROM_HERE,
      WTF::BindOnce(&ResolverWithAbortSignal<IDLDouble>::Resolve<double>,
                    WrapPersistent(resolver), 0));

  return resolver->Promise();
}

double Translator::inputQuota() const {
  return std::numeric_limits<double>::infinity();
}

}  // namespace blink

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/on_device_translation/translator.h"

#include <limits>

#include "base/functional/callback_helpers.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom-blink.h"
#include "third_party/blink/public/mojom/on_device_translation/translator.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
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

bool ValidateAndCanonicalizeSourceAndTargetLanguages(
    v8::Isolate* isolate,
    TranslatorCreateCoreOptions* options) {
  CHECK(options->hasSourceLanguage());
  CHECK(options->hasTargetLanguage());

  v8::Maybe<std::string> canonicalized_source_language =
      isolate->ValidateAndCanonicalizeUnicodeLocaleId(
          options->sourceLanguage().Ascii());
  if (canonicalized_source_language.IsNothing()) {
    return false;
  }

  v8::Maybe<std::string> canonicalized_target_language =
      isolate->ValidateAndCanonicalizeUnicodeLocaleId(
          options->targetLanguage().Ascii());
  if (canonicalized_target_language.IsNothing()) {
    return false;
  }

  options->setSourceLanguage(String(canonicalized_source_language.FromJust()));
  options->setTargetLanguage(String(canonicalized_target_language.FromJust()));
  return true;
}

}  // namespace

Translator::Translator(
    ScriptState* script_state,
    mojo::PendingRemote<mojom::blink::Translator> pending_remote,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    String source_language,
    String target_language,
    AbortSignal* abort_signal)
    : task_runner_(std::move(task_runner)),
      source_language_(std::move(source_language)),
      target_language_(std::move(target_language)),
      destruction_abort_controller_(AbortController::Create(script_state)),
      create_abort_signal_(abort_signal) {
  translator_remote_.Bind(std::move(pending_remote), task_runner_);

  if (create_abort_signal_) {
    CHECK(!create_abort_signal_->aborted());
    create_abort_handle_ = create_abort_signal_->AddAlgorithm(
        BindOnce(&Translator::OnCreateAbortSignalAborted,
                 WrapWeakPersistent(this), WrapWeakPersistent(script_state)));
  }
}

void Translator::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(translator_remote_);
  visitor->Trace(destruction_abort_controller_);
  visitor->Trace(create_abort_signal_);
  visitor->Trace(create_abort_handle_);
}

String Translator::sourceLanguage() const {
  return source_language_;
}
String Translator::targetLanguage() const {
  return target_language_;
}

ScriptPromise<V8Availability> Translator::availability(
    ScriptState* script_state,
    TranslatorCreateCoreOptions* options,
    ExceptionState& exception_state) {
  ExecutionContext* context = ExecutionContext::From(script_state);

  if (!ValidateScriptState(
          script_state, exception_state,
          RuntimeEnabledFeatures::TranslationAPIForWorkersEnabled(context))) {
    return ScriptPromise<V8Availability>();
  }

  if (!ValidateAndCanonicalizeSourceAndTargetLanguages(
          script_state->GetIsolate(), options)) {
    return EmptyPromise();
  }

  ScriptPromiseResolver<V8Availability>* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<V8Availability>>(script_state);
  ScriptPromise<V8Availability> promise = resolver->Promise();

  // Return unavailable if the Permission Policy is not enabled.
  if (!context->IsFeatureEnabled(
          network::mojom::PermissionsPolicyFeature::kTranslator)) {
    resolver->Resolve(AvailabilityToV8(Availability::kUnavailable));
    return promise;
  }

  AIInterfaceProxy::GetTranslationManagerRemote(context)->TranslationAvailable(
      mojom::blink::TranslatorLanguageCode::New(options->sourceLanguage()),
      mojom::blink::TranslatorLanguageCode::New(options->targetLanguage()),
      BindOnce(
          [](ExecutionContext* context,
             ScriptPromiseResolver<V8Availability>* resolver,
             CanCreateTranslatorResult result) {
            CHECK(resolver);

            Availability availability =
                HandleTranslatorAvailabilityCheckResult(context, result);
            resolver->Resolve(AvailabilityToV8(availability));
          },
          WrapPersistent(context), WrapPersistent(resolver)));

  return promise;
}

ScriptPromise<Translator> Translator::create(ScriptState* script_state,
                                             TranslatorCreateOptions* options,
                                             ExceptionState& exception_state) {
  // If `sourceLanguage` and `targetLanguage` are not passed, A TypeError should
  // be thrown before we get here.
  CHECK(options && options->sourceLanguage() && options->targetLanguage());

  ExecutionContext* context = ExecutionContext::From(script_state);

  if (!ValidateScriptState(
          script_state, exception_state,
          RuntimeEnabledFeatures::TranslationAPIForWorkersEnabled(context))) {
    return EmptyPromise();
  }

  if (!ValidateAndCanonicalizeSourceAndTargetLanguages(
          script_state->GetIsolate(), options)) {
    return EmptyPromise();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<Translator>>(script_state);

  // Block access if the Permission Policy is not enabled.
  if (!context->IsFeatureEnabled(
          network::mojom::PermissionsPolicyFeature::kTranslator)) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotAllowedError, kExceptionMessagePermissionPolicy));
    return resolver->Promise();
  }

  AbortSignal* signal = options->getSignalOr(nullptr);
  if (HandleAbortSignal(signal, script_state, exception_state)) {
    return EmptyPromise();
  }

  MakeGarbageCollected<CreateTranslatorClient>(script_state, options, resolver);

  return resolver->Promise();
}

ScriptPromise<IDLString> Translator::translate(
    ScriptState* script_state,
    const String& input,
    TranslatorTranslateOptions* options,
    ExceptionState& exception_state) {
  ExecutionContext* context = ExecutionContext::From(script_state);

  if (!ValidateScriptState(
          script_state, exception_state,
          RuntimeEnabledFeatures::TranslationAPIForWorkersEnabled(context))) {
    return EmptyPromise();
  }

  AbortSignal* composite_signal = CreateCompositeSignal(script_state, options);
  if (HandleAbortSignal(composite_signal, script_state, exception_state)) {
    return EmptyPromise();
  }

  CHECK(options);
  ScriptPromiseResolver<IDLString>* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLString>>(script_state);
  ScriptPromise<IDLString> promise = resolver->Promise();

  // Pass persistent refs to keep this instance alive during the response.
  auto pending_remote = CreateModelExecutionResponder(
      script_state, composite_signal, task_runner_,
      AIMetrics::AISessionType::kTranslator,
      BindOnce(&ResolvePromiseOnCompletion<IDLString>,
               WrapPersistent(resolver)),
      /*overflow_callback=*/base::DoNothingWithBoundArgs(WrapPersistent(this)),
      BindOnce(&RejectPromiseOnError<IDLString>, WrapPersistent(resolver)),
      BindOnce(&RejectPromiseOnAbort<IDLString>, WrapPersistent(resolver),
               WrapPersistent(composite_signal), WrapPersistent(script_state)));

  // TODO(crbug.com/335374928): implement the error handling for the translation
  // service crash.
  translator_remote_->Translate(input, std::move(pending_remote));

  return promise;
}

ReadableStream* Translator::translateStreaming(
    ScriptState* script_state,
    const String& input,
    TranslatorTranslateOptions* options,
    ExceptionState& exception_state) {
  ExecutionContext* context = ExecutionContext::From(script_state);

  if (!ValidateScriptState(
          script_state, exception_state,
          RuntimeEnabledFeatures::TranslationAPIForWorkersEnabled(context))) {
    return nullptr;
  }

  CHECK(options);
  AbortSignal* composite_signal = CreateCompositeSignal(script_state, options);
  if (HandleAbortSignal(composite_signal, script_state, exception_state)) {
    return nullptr;
  }

  // Pass persistent refs to keep this instance alive during the response.
  auto [readable_stream, pending_remote] =
      CreateModelExecutionStreamingResponder(
          script_state, composite_signal, task_runner_,
          AIMetrics::AISessionType::kTranslator,
          /*complete_callback=*/
          base::DoNothingWithBoundArgs(WrapPersistent(this)),
          /*overflow_callback=*/
          base::DoNothingWithBoundArgs(WrapPersistent(this)));

  // TODO(crbug.com/335374928): Implement the error handling for the translation
  // service crash.
  translator_remote_->TranslateStreaming(input, std::move(pending_remote));

  return readable_stream;
}

void Translator::destroy(ScriptState* script_state) {
  destruction_abort_controller_->abort(script_state);
  DestroyImpl();
}

void Translator::DestroyImpl() {
  translator_remote_.reset();
  if (create_abort_handle_) {
    create_abort_signal_->RemoveAlgorithm(create_abort_handle_);
    create_abort_handle_ = nullptr;
  }
}

void Translator::OnCreateAbortSignalAborted(ScriptState* script_state) {
  if (script_state) {
    destruction_abort_controller_->abort(
        script_state, create_abort_signal_->reason(script_state));
  }
  DestroyImpl();
}

ScriptPromise<IDLDouble> Translator::measureInputUsage(
    ScriptState* script_state,
    const String& input,
    TranslatorTranslateOptions* options,
    ExceptionState& exception_state) {
  ExecutionContext* context = ExecutionContext::From(script_state);

  if (!ValidateScriptState(
          script_state, exception_state,
          RuntimeEnabledFeatures::TranslationAPIForWorkersEnabled(context))) {
    return EmptyPromise();
  }

  CHECK(options);
  AbortSignal* composite_signal = CreateCompositeSignal(script_state, options);
  if (HandleAbortSignal(composite_signal, script_state, exception_state)) {
    return EmptyPromise();
  }

  ResolverWithAbortSignal<IDLDouble>* resolver =
      MakeGarbageCollected<ResolverWithAbortSignal<IDLDouble>>(
          script_state, composite_signal);

  task_runner_->PostTask(
      FROM_HERE, BindOnce(&ResolverWithAbortSignal<IDLDouble>::Resolve<double>,
                          WrapPersistent(resolver), 0));

  return resolver->Promise();
}

double Translator::inputQuota() const {
  return std::numeric_limits<double>::infinity();
}

AbortSignal* Translator::CreateCompositeSignal(
    ScriptState* script_state,
    TranslatorTranslateOptions* options) {
  HeapVector<Member<AbortSignal>> signals;

  signals.push_back(destruction_abort_controller_->signal());

  CHECK(options);
  if (options->hasSignal()) {
    signals.push_back(options->signal());
  }

  return MakeGarbageCollected<AbortSignal>(script_state, signals);
}

}  // namespace blink

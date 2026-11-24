// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/on_device_translation/translator.h"

#include <unicode/utf.h>
#include <unicode/utf8.h>

#include <cstdint>
#include <limits>

#include "base/check_deref.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom-blink.h"
#include "third_party/blink/public/mojom/on_device_translation/translator.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/modules/ai/ai_interface_proxy.h"
#include "third_party/blink/renderer/modules/ai/ai_metrics.h"
#include "third_party/blink/renderer/modules/ai/exception_helpers.h"
#include "third_party/blink/renderer/modules/ai/feedback_helpers.h"
#include "third_party/blink/renderer/modules/ai/model_execution_responder.h"
#include "third_party/blink/renderer/modules/ai/on_device_translation/create_translator_client.h"
#include "third_party/blink/renderer/modules/ai/resolver_with_abort_signal.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/code_point_iterator.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"

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

// Computes the UTF-8 byte length of `ReplaceUnmatchedSurrogates(input)`
// without allocating a UTF-8 buffer.
uint64_t MeasureUtf8ByteLength(const String& input) {
  if (input.empty()) {
    return 0;
  }
  uint64_t length = 0;
  for (UChar32 cp : StringView(input)) {
    // U8_LENGTH returns 0 for surrogates, but ReplaceUnmatchedSurrogates()
    // replaces unmatched surrogates with U+FFFD (3 UTF-8 bytes).
    length += U_IS_SURROGATE(cp) ? 3 : U8_LENGTH(cp);
  }
  return length;
}

}  // namespace

Translator::Translator(
    ScriptState* script_state,
    mojo::PendingRemote<mojom::blink::Translator> pending_remote,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    String source_language,
    String target_language,
    AbortSignal* abort_signal,
    std::optional<uint64_t> input_quota)
    : task_runner_(std::move(task_runner)),
      source_language_(std::move(source_language)),
      target_language_(std::move(target_language)),
      input_quota_(input_quota),
      abort_manager_(script_state, abort_signal, this) {
  translator_remote_.Bind(std::move(pending_remote), task_runner_);
}

void Translator::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(translator_remote_);
  visitor->Trace(abort_manager_);
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
  MaybeRequestFeedback(script_state, AIMetrics::AISessionType::kTranslator);
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

  auto* signal = abort_manager_.CreateSignal(
      script_state, CHECK_DEREF(options).getSignalOr(nullptr), exception_state);
  if (!signal) {
    return EmptyPromise();
  }

  base::UmaHistogramCounts1M(AIMetrics::GetAISessionRequestSizeMetricName(
                                 AIMetrics::AISessionType::kTranslator),
                             static_cast<int>(input.length()));

  ScriptPromiseResolver<IDLString>* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLString>>(script_state);
  ScriptPromise<IDLString> promise = resolver->Promise();

  // Pass persistent refs to keep this instance alive during the response.
  auto pending_remote = CreateModelExecutionResponder(
      script_state, signal, task_runner_, AIMetrics::AISessionType::kTranslator,
      BindOnce(&ResolvePromiseOnCompletion<IDLString>,
               WrapPersistent(resolver)),
      /*tool_call_callback=*/base::NullCallback(),
      /*overflow_callback=*/base::DoNothingWithBoundArgs(WrapPersistent(this)),
      BindOnce(&RejectPromiseOnError<IDLString>, WrapPersistent(resolver)),
      BindOnce(&RejectPromiseOnAbort<IDLString>, WrapPersistent(resolver),
               WrapPersistent(signal), WrapPersistent(script_state)));

  // TODO(crbug.com/335374928): implement the error handling for the translation
  // service crash.
  translator_remote_->Translate(ReplaceUnmatchedSurrogates(input),
                                std::move(pending_remote));

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

  auto* signal = abort_manager_.CreateSignal(
      script_state, CHECK_DEREF(options).getSignalOr(nullptr), exception_state);
  if (!signal) {
    return nullptr;
  }

  base::UmaHistogramCounts1M(AIMetrics::GetAISessionRequestSizeMetricName(
                                 AIMetrics::AISessionType::kTranslator),
                             static_cast<int>(input.length()));

  // Pass persistent refs to keep this instance alive during the response.
  auto [readable_stream, pending_remote] =
      CreateModelExecutionStreamingResponder(
          script_state, signal, task_runner_,
          AIMetrics::AISessionType::kTranslator,
          /*complete_callback=*/
          base::DoNothingWithBoundArgs(WrapPersistent(this)),
          /*overflow_callback=*/
          base::DoNothingWithBoundArgs(WrapPersistent(this)));

  // TODO(crbug.com/335374928): Implement the error handling for the translation
  // service crash.
  translator_remote_->TranslateStreaming(ReplaceUnmatchedSurrogates(input),
                                         std::move(pending_remote));

  return readable_stream;
}

void Translator::destroy(ScriptState* script_state) {
  // TODO(crbug.com/468104678): Align destroy() RaisesException behavior.
  if (abort_manager_.OnDestroy(script_state, IGNORE_EXCEPTION)) {
    translator_remote_.reset();
  }
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

  auto* signal = abort_manager_.CreateSignal(
      script_state, CHECK_DEREF(options).getSignalOr(nullptr), exception_state);
  if (!signal) {
    return EmptyPromise();
  }

  auto* resolver = MakeGarbageCollected<ResolverWithAbortSignal<IDLDouble>>(
      script_state, signal);

  // The browser enforces `TranslationAPIMaxInputSizeBytes` on the UTF-8 string
  // sent over Mojo, so measure usage in UTF-8 bytes to match `inputQuota` and
  // `QuotaExceededError.requested`.
  const double usage =
      input_quota_ ? base::strict_cast<double>(MeasureUtf8ByteLength(input))
                   : 0.0;
  task_runner_->PostTask(
      FROM_HERE, BindOnce(&ResolverWithAbortSignal<IDLDouble>::Resolve<double>,
                          WrapPersistent(resolver), usage));

  return resolver->Promise();
}

double Translator::inputQuota() const {
  return input_quota_ ? base::strict_cast<double>(*input_quota_)
                      : std::numeric_limits<double>::infinity();
}

}  // namespace blink

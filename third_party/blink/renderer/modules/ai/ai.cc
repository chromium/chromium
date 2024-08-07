// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/ai.h"

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/metrics/histogram_functions.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_model_availability.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_text_model_info.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_text_session_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/ai/ai_metrics.h"
#include "third_party/blink/renderer/modules/ai/ai_text_session.h"
#include "third_party/blink/renderer/modules/ai/exception_helpers.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

AI::AI(ExecutionContext* context)
    : ExecutionContextClient(context),
      task_runner_(context->GetTaskRunner(TaskType::kInternalDefault)),
      ai_remote_(context),
      text_session_factory_(
          MakeGarbageCollected<AITextSessionFactory>(context, task_runner_)) {}

void AI::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(ai_remote_);
  visitor->Trace(text_session_factory_);
  visitor->Trace(ai_summarizer_factory_);
}

HeapMojoRemote<mojom::blink::AIManager>& AI::GetAIRemote() {
  if (!ai_remote_.is_bound()) {
    if (GetExecutionContext()) {
      GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
          ai_remote_.BindNewPipeAndPassReceiver(task_runner_));
    }
  }
  return ai_remote_;
}

ScriptPromise<V8AIModelAvailability> AI::canCreateTextSession(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return ScriptPromise<V8AIModelAvailability>();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<V8AIModelAvailability>>(
          script_state);
  auto promise = resolver->Promise();
  using ModelAvailabilityCheckResult =
      mojom::blink::ModelAvailabilityCheckResult;

  text_session_factory_->CanCreateTextSession(WTF::BindOnce(
      [](ScriptPromiseResolver<V8AIModelAvailability>* resolver,
         AIModelAvailability availability,
         ModelAvailabilityCheckResult check_result) {
        resolver->Resolve(AIModelAvailabilityToV8(availability));
      },
      WrapPersistent(resolver)));
  return promise;
}

ScriptPromise<AITextSession> AI::createTextSession(
    ScriptState* script_state,
    AITextSessionOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return ScriptPromise<AITextSession>();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<AITextSession>>(script_state);
  auto promise = resolver->Promise();
  mojom::blink::AITextSessionSamplingParamsPtr sampling_params;
  WTF::String system_prompt;
  if (options) {
    if (!options->hasTopK() && !options->hasTemperature()) {
      sampling_params = nullptr;
    } else if (options->hasTopK() && options->hasTemperature()) {
      sampling_params = mojom::blink::AITextSessionSamplingParams::New(
          options->topK(), options->temperature());
    } else {
      resolver->Reject(DOMException::Create(
          kExceptionMessageInvalidTemperatureAndTopKFormat,
          DOMException::GetErrorName(DOMExceptionCode::kNotSupportedError)));
      return promise;
    }
    if (options->hasSystemPrompt()) {
      system_prompt = options->systemPrompt();
    }
  }

  text_session_factory_->CreateTextSession(
      std::move(sampling_params), system_prompt,
      WTF::BindOnce(
          [](ScriptPromiseResolver<AITextSession>* resolver,
             base::expected<AITextSession*, DOMException*> result) {
            if (result.has_value()) {
              resolver->Resolve(result.value());
            } else {
              resolver->Reject(result.error());
            }
          },
          WrapPersistent(resolver)));
  return promise;
}

ScriptPromise<AITextModelInfo> AI::textModelInfo(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return ScriptPromise<AITextModelInfo>();
  }

  base::UmaHistogramEnumeration(
      AIMetrics::GetAIAPIUsageMetricName(AIMetrics::AISessionType::kText),
      AIMetrics::AIAPI::kTextModelInfo);

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<AITextModelInfo>>(
      script_state);
  auto promise = resolver->Promise();

  if (!GetAIRemote().is_connected()) {
    RejectPromiseWithInternalError(resolver);
    return promise;
  }

  GetAIRemote()->GetTextModelInfo(WTF::BindOnce(
      [](ScriptPromiseResolver<AITextModelInfo>* resolver,
         mojom::blink::AITextModelInfoPtr text_model_info) {
        AITextModelInfo* options = AITextModelInfo::Create();
        CHECK(text_model_info);
        options->setDefaultTopK(text_model_info->default_top_k);
        options->setMaxTopK(text_model_info->max_top_k);
        options->setDefaultTemperature(text_model_info->default_temperature);
        resolver->Resolve(options);
      },
      WrapPersistent(resolver)));
  return promise;
}

AISummarizerFactory* AI::summarizer() {
  if (!ai_summarizer_factory_) {
    ai_summarizer_factory_ = MakeGarbageCollected<AISummarizerFactory>(
        GetExecutionContext(), task_runner_);
  }
  return ai_summarizer_factory_.Get();
}

}  // namespace blink

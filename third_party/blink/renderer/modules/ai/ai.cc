// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/ai.h"

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/metrics/histogram_functions.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-blink.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_model_availability.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_text_session_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/ai/ai_metrics.h"
#include "third_party/blink/renderer/modules/ai/ai_text_session.h"
#include "third_party/blink/renderer/modules/ai/exception_helpers.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

V8AIModelAvailability AvailabilityToV8(AI::ModelAvailability availability) {
  switch (availability) {
    case AI::ModelAvailability::kReadily:
      return V8AIModelAvailability(V8AIModelAvailability::Enum::kReadily);
    case AI::ModelAvailability::kAfterDownload:
      return V8AIModelAvailability(V8AIModelAvailability::Enum::kAfterDownload);
    case AI::ModelAvailability::kNo:
      return V8AIModelAvailability(V8AIModelAvailability::Enum::kNo);
  }
}

void ResolveAvailability(ScriptPromiseResolver<V8AIModelAvailability>* resolver,
                         AI::ModelAvailability availability) {
  base::UmaHistogramEnumeration(AIMetrics::GetAIModelAvailabilityMetricName(
                                    AIMetrics::AISessionType::kText),
                                availability);
  resolver->Resolve(AvailabilityToV8(availability));
}

}  // namespace

AI::AI(ExecutionContext* context)
    : ExecutionContextClient(context),
      task_runner_(context->GetTaskRunner(TaskType::kInternalDefault)),
      ai_remote_(context) {}

void AI::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(ai_remote_);
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

  base::UmaHistogramEnumeration(
      AIMetrics::GetAIAPIUsageMetricName(AIMetrics::AISessionType::kText),
      AIMetrics::AIAPI::kCanCreateSession);

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<V8AIModelAvailability>>(
          script_state);
  auto promise = resolver->Promise();

  if (!GetAIRemote().is_connected()) {
    ResolveAvailability(resolver, ModelAvailability::kNo);
    return promise;
  }

  GetAIRemote()->CanCreateTextSession(WTF::BindOnce(
      [](ScriptPromiseResolver<V8AIModelAvailability>* resolver, AI* ai,
         mojom::blink::ModelAvailabilityCheckResult result) {
        if (result == mojom::blink::ModelAvailabilityCheckResult::kReadily) {
          ResolveAvailability(resolver, ModelAvailability::kReadily);
        } else if (result ==
                   mojom::blink::ModelAvailabilityCheckResult::kAfterDownload) {
          // TODO(crbug.com/345357441): Implement the
          // `ontextmodeldownloadprogress` event.
          ResolveAvailability(resolver, ModelAvailability::kAfterDownload);
        } else {
          // If the text session cannot be created, logs the error message to
          // the console.
          ai->GetExecutionContext()->AddConsoleMessage(
              mojom::blink::ConsoleMessageSource::kJavaScript,
              mojom::blink::ConsoleMessageLevel::kWarning,
              ConvertModelAvailabilityCheckResultToDebugString(result));
          ResolveAvailability(resolver, ModelAvailability::kNo);
        }
      },
      WrapPersistent(resolver), WrapWeakPersistent(this)));

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

  base::UmaHistogramEnumeration(
      AIMetrics::GetAIAPIUsageMetricName(AIMetrics::AISessionType::kText),
      AIMetrics::AIAPI::kCreateSession);

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<AITextSession>>(script_state);
  auto promise = resolver->Promise();

  if (!GetAIRemote().is_connected()) {
    RejectPromiseWithInternalError(resolver);
    return promise;
  }

  mojom::blink::AITextSessionSamplingParamsPtr sampling_params;
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
  }

  AITextSession* text_session =
      MakeGarbageCollected<AITextSession>(GetExecutionContext(), task_runner_);
  GetAIRemote()->CreateTextSession(
      text_session->GetModelSessionReceiver(), std::move(sampling_params),
      WTF::BindOnce(
          [](ScriptPromiseResolver<AITextSession>* resolver,
             AITextSession* text_session, bool success) {
            if (success) {
              resolver->Resolve(text_session);
            } else {
              resolver->Reject(DOMException::Create(
                  kExceptionMessageUnableToCreateSession,
                  DOMException::GetErrorName(
                      DOMExceptionCode::kInvalidStateError)));
            }
          },
          WrapPersistent(resolver), WrapPersistent(text_session)));

  return promise;
}

ScriptPromise<AITextSessionOptions> AI::defaultTextSessionOptions(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return ScriptPromise<AITextSessionOptions>();
  }

  base::UmaHistogramEnumeration(
      AIMetrics::GetAIAPIUsageMetricName(AIMetrics::AISessionType::kText),
      AIMetrics::AIAPI::kDefaultTextSessionOptions);

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<AITextSessionOptions>>(
          script_state);
  auto promise = resolver->Promise();

  if (!GetAIRemote().is_connected()) {
    RejectPromiseWithInternalError(resolver);
    return promise;
  }

  GetAIRemote()->GetDefaultTextSessionSamplingParams(WTF::BindOnce(
      [](ScriptPromiseResolver<AITextSessionOptions>* resolver,
         mojom::blink::AITextSessionSamplingParamsPtr default_params) {
        AITextSessionOptions* options = AITextSessionOptions::Create();
        CHECK(default_params);
        options->setTopK(default_params->top_k);
        options->setTemperature(default_params->temperature);
        resolver->Resolve(options);
      },
      WrapPersistent(resolver)));

  return promise;
}

}  // namespace blink

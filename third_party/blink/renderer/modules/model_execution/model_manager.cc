// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/model_execution/model_manager.h"

#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/public/mojom/model_execution/model_manager.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/model_execution/model_manager.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_generic_model_availability.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_model_generic_session_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/modules/model_execution/model_execution_metrics.h"
#include "third_party/blink/renderer/modules/model_execution/model_generic_session.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

V8GenericModelAvailability AvailabilityToV8(
    ModelManager::ModelAvailability availability) {
  switch (availability) {
    case ModelManager::ModelAvailability::kReadily:
      return V8GenericModelAvailability(
          V8GenericModelAvailability::Enum::kReadily);
    case ModelManager::ModelAvailability::kAfterDownload:
      return V8GenericModelAvailability(
          V8GenericModelAvailability::Enum::kAfterDownload);
    case ModelManager::ModelAvailability::kNo:
      return V8GenericModelAvailability(V8GenericModelAvailability::Enum::kNo);
  }
}

ModelManager::ModelManager(LocalDOMWindow* window)
    : ExecutionContextClient(window),
      task_runner_(window->GetTaskRunner(TaskType::kInternalDefault)) {}

void ModelManager::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(model_manager_remote_);
}

HeapMojoRemote<mojom::blink::ModelManager>&
ModelManager::GetModelManagerRemote() {
  if (!model_manager_remote_.is_bound()) {
    if (DomWindow() && DomWindow()->GetFrame()) {
      DomWindow()->GetFrame()->GetBrowserInterfaceBroker().GetInterface(
          model_manager_remote_.BindNewPipeAndPassReceiver(task_runner_));
    }
  }
  return model_manager_remote_;
}

void ResolveAvailability(
    ScriptPromiseResolver<V8GenericModelAvailability>* resolver,
    ModelManager::ModelAvailability availability) {
  base::UmaHistogramEnumeration(
      ModelExecutionMetrics::GetModelExecutionAvailabilityMetricName(
          ModelExecutionMetrics::ModelExecutionSessionType::kGeneric),
      availability);
  resolver->Resolve(AvailabilityToV8(availability));
}

ScriptPromise<V8GenericModelAvailability> ModelManager::canCreateGenericSession(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The execution context is not valid.");
    return ScriptPromise<V8GenericModelAvailability>();
  }

  base::UmaHistogramEnumeration(
      ModelExecutionMetrics::GetModelExecutionAPIUsageMetricName(
          ModelExecutionMetrics::ModelExecutionSessionType::kGeneric),
      ModelExecutionMetrics::ModelExecutionAPI::kModelCanCreateSession);

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<V8GenericModelAvailability>>(
          script_state);
  auto promise = resolver->Promise();

  if (!GetModelManagerRemote().is_connected()) {
    ResolveAvailability(resolver, ModelAvailability::kNo);
  } else {
    GetModelManagerRemote()->CanCreateGenericSession(WTF::BindOnce(
        [](ScriptPromiseResolver<V8GenericModelAvailability>* resolver,
           bool can_create) {
          ModelAvailability availability = ModelAvailability::kNo;
          if (can_create) {
            availability = ModelAvailability::kReadily;
          }
          ResolveAvailability(resolver, availability);
        },
        WrapPersistent(resolver)));
  }

  return promise;
}

ScriptPromise<ModelGenericSession> ModelManager::createGenericSession(
    ScriptState* script_state,
    ModelGenericSessionOptions* options,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid() ||
      !GetModelManagerRemote().is_connected()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The execution context is not valid.");
    return ScriptPromise<ModelGenericSession>();
  }

  base::UmaHistogramEnumeration(
      ModelExecutionMetrics::GetModelExecutionAPIUsageMetricName(
          ModelExecutionMetrics::ModelExecutionSessionType::kGeneric),
      ModelExecutionMetrics::ModelExecutionAPI::kModelCreateSession);

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<ModelGenericSession>>(
          script_state);
  auto promise = resolver->Promise();

  mojom::blink::ModelGenericSessionSamplingParamsPtr sampling_params;
  if (options) {
    if (!options->hasTopK() && !options->hasTemperature()) {
      sampling_params = nullptr;
    } else if (options->hasTopK() && options->hasTemperature()) {
      sampling_params = mojom::blink::ModelGenericSessionSamplingParams::New(
          options->topK(), options->temperature());
    } else {
      exception_state.ThrowTypeError(
          "Initializing a new session must either specify both topK and "
          "temperature, or neither of them.");
      return ScriptPromise<ModelGenericSession>();
    }
  }

  ModelGenericSession* generic_session =
      MakeGarbageCollected<ModelGenericSession>(task_runner_);
  GetModelManagerRemote()->CreateGenericSession(
      generic_session->GetModelSessionReceiver(), std::move(sampling_params),
      WTF::BindOnce(
          [](ScriptPromiseResolver<ModelGenericSession>* resolver,
             ModelGenericSession* generic_session, bool success) {
            if (success) {
              resolver->Resolve(generic_session);
            } else {
              resolver->Reject();
            }
          },
          WrapPersistent(resolver), WrapPersistent(generic_session)));

  return promise;
}

ScriptPromise<ModelGenericSessionOptions>
ModelManager::defaultGenericSessionOptions(ScriptState* script_state,
                                           ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The execution context is not valid.");
    return ScriptPromise<ModelGenericSessionOptions>();
  }

  base::UmaHistogramEnumeration(
      ModelExecutionMetrics::GetModelExecutionAPIUsageMetricName(
          ModelExecutionMetrics::ModelExecutionSessionType::kGeneric),
      ModelExecutionMetrics::ModelExecutionAPI::
          kModelDefaultGenericSessionOptions);

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<ModelGenericSessionOptions>>(
          script_state);
  auto promise = resolver->Promise();

  if (!GetModelManagerRemote().is_connected()) {
    resolver->Reject(DOMException::Create(
        "Unable to fetch default generic session options", "NotReadableError"));
  } else {
    GetModelManagerRemote()->GetDefaultGenericSessionSamplingParams(
        WTF::BindOnce(
            [](ScriptPromiseResolver<ModelGenericSessionOptions>* resolver,
               mojom::blink::ModelGenericSessionSamplingParamsPtr
                   default_params) {
              ModelGenericSessionOptions* options =
                  ModelGenericSessionOptions::Create();
              CHECK(default_params);
              options->setTopK(default_params->top_k);
              options->setTemperature(default_params->temperature);
              resolver->Resolve(options);
            },
            WrapPersistent(resolver)));
  }

  return promise;
}

}  // namespace blink

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/model_execution/model_manager.h"

#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/modules/model_execution/model_execution_metrics.h"
#include "third_party/blink/renderer/modules/model_execution/model_generic_session.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

String AvailabilityToString(ModelManager::ModelAvailability availability) {
  DEFINE_STATIC_LOCAL(const String, readily, ("readily"));
  DEFINE_STATIC_LOCAL(const String, after_download, ("after-download"));
  DEFINE_STATIC_LOCAL(const String, no, ("no"));

  switch (availability) {
    case ModelManager::ModelAvailability::kReadily:
      return readily;
    case ModelManager::ModelAvailability::kAfterDownload:
      return after_download;
    case ModelManager::ModelAvailability::kNo:
      return no;
  }

  NOTREACHED();
  return String();
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

void ResolveAvailability(ScriptPromiseResolver* resolver,
                         ModelManager::ModelAvailability availability) {
  base::UmaHistogramEnumeration(
      ModelExecutionMetrics::GetModelExecutionAvailabilityMetricName(
          ModelExecutionMetrics::ModelExecutionSessionType::kGeneric),
      availability);
  resolver->Resolve(AvailabilityToString(availability));
}

ScriptPromise ModelManager::canCreateGenericSession(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The execution context is not valid.");
    return ScriptPromise();
  }

  base::UmaHistogramEnumeration(
      ModelExecutionMetrics::GetModelExecutionAPIUsageMetricName(
          ModelExecutionMetrics::ModelExecutionSessionType::kGeneric),
      ModelExecutionMetrics::ModelExecutionAPI::kModelCanCreateSession);

  ScriptPromiseResolver* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  if (!GetModelManagerRemote().is_connected()) {
    ResolveAvailability(resolver, ModelAvailability::kNo);
  } else {
    GetModelManagerRemote()->CanCreateGenericSession(WTF::BindOnce(
        [](ScriptPromiseResolver* resolver, bool can_create) {
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

ScriptPromise ModelManager::createGenericSession(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid() ||
      !GetModelManagerRemote().is_connected()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The execution context is not valid.");
    return ScriptPromise();
  }

  base::UmaHistogramEnumeration(
      ModelExecutionMetrics::GetModelExecutionAPIUsageMetricName(
          ModelExecutionMetrics::ModelExecutionSessionType::kGeneric),
      ModelExecutionMetrics::ModelExecutionAPI::kModelCreateSession);

  ScriptPromiseResolver* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  ModelGenericSession* generic_session =
      MakeGarbageCollected<ModelGenericSession>(task_runner_);
  GetModelManagerRemote()->CreateGenericSession(
      generic_session->GetModelSessionReceiver(),
      WTF::BindOnce(
          [](ScriptPromiseResolver* resolver,
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

}  // namespace blink

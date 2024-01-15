// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/model_execution/model_manager.h"

#include "base/functional/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/modules/model_execution/model_generic_session.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

String AvailabilityToString(ModelManager::Availability availability) {
  DEFINE_STATIC_LOCAL(const String, readily, ("readily"));
  DEFINE_STATIC_LOCAL(const String, after_download, ("after-download"));
  DEFINE_STATIC_LOCAL(const String, no, ("no"));

  switch (availability) {
    case ModelManager::kReadily:
      return readily;
    case ModelManager::kAfterDownload:
      return after_download;
    case ModelManager::kNo:
      return no;
  }

  NOTREACHED();
  return String();
}

ModelManager::ModelManager(LocalDOMWindow* window)
    : task_runner_(window->GetTaskRunner(TaskType::kInternalDefault)) {
  CHECK(window && window->GetFrame());
  window->GetFrame()->GetBrowserInterfaceBroker().GetInterface(
      model_manager_remote_.BindNewPipeAndPassReceiver(task_runner_));
}

void ModelManager::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(model_manager_remote_);
}

ScriptPromise ModelManager::canCreateGenericSession(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The execution context is not valid.");
    return ScriptPromise();
  }

  ScriptPromiseResolver* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  // TODO(leimy): in the future, we may need to check if the model has been
  // downloaded etc.
  if (!model_manager_remote_.is_connected()) {
    resolver->Resolve(AvailabilityToString(kNo));
  } else {
    model_manager_remote_->CanCreateGenericSession(WTF::BindOnce(
        [](ScriptPromiseResolver* resolver, bool can_create) {
          Availability availability = kNo;
          if (can_create) {
            availability = kReadily;
          }
          resolver->Resolve(AvailabilityToString(availability));
        },
        WrapPersistent(resolver)));
  }

  return promise;
}

ScriptPromise ModelManager::createGenericSession(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The execution context is not valid.");
    return ScriptPromise();
  }

  ScriptPromiseResolver* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  ModelGenericSession* generic_session =
      MakeGarbageCollected<ModelGenericSession>(task_runner_);
  model_manager_remote_->CreateGenericSession(
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

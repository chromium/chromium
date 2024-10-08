// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/background_sync/periodic_sync_manager.h"

#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_background_sync_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_registration.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

PeriodicSyncManager::PeriodicSyncManager(
    ServiceWorkerRegistration* registration,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : registration_(registration),
      task_runner_(std::move(task_runner)),
      background_sync_service_(registration_->GetExecutionContext()) {
  DCHECK(registration_);
}

ScriptPromise<IDLUndefined> PeriodicSyncManager::registerPeriodicSync(
    ScriptState* script_state,
    const String& tag,
    const BackgroundSyncOptions* options,
    ExceptionState& exception_state) {
  if (!registration_->active()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Registration failed - no active Service Worker");
    return EmptyPromise();
  }

  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  if (execution_context->IsInFencedFrame()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "Periodic Background Sync is not allowed in fenced frames.");
    return EmptyPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  mojom::blink::SyncRegistrationOptionsPtr sync_registration =
      mojom::blink::SyncRegistrationOptions::New(tag, options->minInterval());

  GetBackgroundSyncServiceRemote()->Register(
      std::move(sync_registration), registration_->RegistrationId(),
      resolver->WrapCallbackInScriptScope(WTF::BindOnce(
          &PeriodicSyncManager::RegisterCallback, WrapPersistent(this))));

  return promise;
}

ScriptPromise<IDLSequence<IDLString>> PeriodicSyncManager::getTags(
    ScriptState* script_state) {
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  if (execution_context->IsInFencedFrame()) {
    return ScriptPromise<IDLSequence<IDLString>>::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kNotAllowedError,
            "Periodic Background Sync is not allowed in fenced frames."));
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLSequence<IDLString>>>(
          script_state);
  auto promise = resolver->Promise();

  // Creating a Periodic Background Sync registration requires an activated
  // service worker, so if |registration_| has not been activated yet, we can
  // skip the Mojo roundtrip.
  if (!registration_->active()) {
    resolver->Resolve(Vector<String>());
  } else {
    // TODO(crbug.com/932591): Optimize this to only get the tags from the
    // browser process instead of the registrations themselves.
    GetBackgroundSyncServiceRemote()->GetRegistrations(
        registration_->RegistrationId(),
        resolver->WrapCallbackInScriptScope(
            WTF::BindOnce(&PeriodicSyncManager::GetRegistrationsCallback,
                          WrapPersistent(this))));
  }
  return promise;
}

ScriptPromise<IDLUndefined> PeriodicSyncManager::unregister(
    ScriptState* script_state,
    const String& tag) {
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  if (execution_context->IsInFencedFrame()) {
    return ScriptPromise<IDLUndefined>::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kNotAllowedError,
            "Periodic Background Sync is not allowed in fenced frames."));
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  auto promise = resolver->Promise();

  // Silently succeed if there's no active service worker registration.
  if (!registration_->active()) {
    resolver->Resolve();
    return promise;
  }

  GetBackgroundSyncServiceRemote()->Unregister(
      registration_->RegistrationId(), tag,
      resolver->WrapCallbackInScriptScope(WTF::BindOnce(
          &PeriodicSyncManager::UnregisterCallback, WrapPersistent(this))));
  return promise;
}

mojom::blink::PeriodicBackgroundSyncService*
PeriodicSyncManager::GetBackgroundSyncServiceRemote() {
  if (!background_sync_service_.is_bound()) {
    registration_->GetExecutionContext()
        ->GetBrowserInterfaceBroker()
        .GetInterface(
            background_sync_service_.BindNewPipeAndPassReceiver(task_runner_));
  }
  return background_sync_service_.get();
}

void PeriodicSyncManager::RegisterCallback(
    ScriptPromiseResolver<IDLUndefined>* resolver,
    mojom::blink::BackgroundSyncError error,
    mojom::blink::SyncRegistrationOptionsPtr options) {
  switch (error) {
    case mojom::blink::BackgroundSyncError::NONE:
      resolver->Resolve();
      break;
    case mojom::blink::BackgroundSyncError::NOT_FOUND:
      NOTREACHED_IN_MIGRATION();
      break;
    case mojom::blink::BackgroundSyncError::STORAGE:
      resolver->Reject(V8ThrowDOMException::CreateOrDie(
          resolver->GetScriptState()->GetIsolate(),
          DOMExceptionCode::kUnknownError, "Unknown error."));
      break;
    case mojom::blink::BackgroundSyncError::NOT_ALLOWED:
      resolver->Reject(V8ThrowDOMException::CreateOrDie(
          resolver->GetScriptState()->GetIsolate(),
          DOMExceptionCode::kInvalidAccessError,
          "Attempted to register a sync event without a "
          "window or registration tag too long."));
      break;
    case mojom::blink::BackgroundSyncError::PERMISSION_DENIED:
      resolver->Reject(V8ThrowDOMException::CreateOrDie(
          resolver->GetScriptState()->GetIsolate(),
          DOMExceptionCode::kNotAllowedError, "Permission denied."));
      break;
    case mojom::blink::BackgroundSyncError::NO_SERVICE_WORKER:
      resolver->Reject(V8ThrowDOMException::CreateOrDie(
          resolver->GetScriptState()->GetIsolate(),
          DOMExceptionCode::kInvalidStateError,
          "Registration failed - no active Service Worker"));
      break;
  }
}

void PeriodicSyncManager::GetRegistrationsCallback(
    ScriptPromiseResolver<IDLSequence<IDLString>>* resolver,
    mojom::blink::BackgroundSyncError error,
    WTF::Vector<mojom::blink::SyncRegistrationOptionsPtr> registrations) {
  switch (error) {
    case mojom::blink::BackgroundSyncError::NONE: {
      Vector<String> tags;
      for (const auto& registration : registrations) {
        tags.push_back(registration->tag);
      }
      resolver->Resolve(std::move(tags));
      break;
    }
    case mojom::blink::BackgroundSyncError::NOT_FOUND:
    case mojom::blink::BackgroundSyncError::NOT_ALLOWED:
    case mojom::blink::BackgroundSyncError::PERMISSION_DENIED:
      // These errors should never be returned from
      // BackgroundSyncManager::GetPeriodicSyncRegistrations
      NOTREACHED_IN_MIGRATION();
      break;
    case mojom::blink::BackgroundSyncError::STORAGE:
      resolver->Reject(V8ThrowDOMException::CreateOrDie(
          resolver->GetScriptState()->GetIsolate(),
          DOMExceptionCode::kUnknownError, "Unknown error."));
      break;
    case mojom::blink::BackgroundSyncError::NO_SERVICE_WORKER:
      resolver->Reject(V8ThrowDOMException::CreateOrDie(
          resolver->GetScriptState()->GetIsolate(),
          DOMExceptionCode::kUnknownError, "No service worker is active."));
      break;
  }
}

void PeriodicSyncManager::UnregisterCallback(
    ScriptPromiseResolver<IDLUndefined>* resolver,
    mojom::blink::BackgroundSyncError error) {
  switch (error) {
    case mojom::blink::BackgroundSyncError::NONE:
      resolver->Resolve();
      break;
    case mojom::blink::BackgroundSyncError::NO_SERVICE_WORKER:
      resolver->Reject(V8ThrowDOMException::CreateOrDie(
          resolver->GetScriptState()->GetIsolate(),
          DOMExceptionCode::kUnknownError, "No service worker is active."));
      break;
    case mojom::blink::BackgroundSyncError::STORAGE:
      resolver->Reject(V8ThrowDOMException::CreateOrDie(
          resolver->GetScriptState()->GetIsolate(),
          DOMExceptionCode::kUnknownError, "Unknown error."));
      break;
    case mojom::blink::BackgroundSyncError::NOT_FOUND:
    case mojom::blink::BackgroundSyncError::NOT_ALLOWED:
    case mojom::BackgroundSyncError::PERMISSION_DENIED:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

void PeriodicSyncManager::Trace(Visitor* visitor) const {
  visitor->Trace(registration_);
  visitor->Trace(background_sync_service_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink

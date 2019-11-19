// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/service_worker/service_worker_registration.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "third_party/blink/public/mojom/loader/fetch_client_settings_object.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/service_worker/navigation_preload_state.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_container.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_error.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_global_scope.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher_properties.h"

namespace blink {

namespace {

void DidUpdate(ScriptPromiseResolver* resolver,
               ServiceWorkerRegistration* registration,
               mojom::ServiceWorkerErrorType error,
               const String& error_msg) {
  if (!resolver->GetExecutionContext() ||
      resolver->GetExecutionContext()->IsContextDestroyed()) {
    return;
  }

  if (error != mojom::ServiceWorkerErrorType::kNone) {
    DCHECK(!error_msg.IsNull());
    ScriptState::Scope scope(resolver->GetScriptState());
    resolver->Reject(ServiceWorkerErrorForUpdate::Take(
        resolver, WebServiceWorkerError(error, error_msg)));
    return;
  }
  resolver->Resolve(registration);
}

void DidUnregister(ScriptPromiseResolver* resolver,
                   mojom::ServiceWorkerErrorType error,
                   const String& error_msg) {
  if (!resolver->GetExecutionContext() ||
      resolver->GetExecutionContext()->IsContextDestroyed()) {
    return;
  }

  if (error != mojom::ServiceWorkerErrorType::kNone &&
      error != mojom::ServiceWorkerErrorType::kNotFound) {
    DCHECK(!error_msg.IsNull());
    resolver->Reject(
        ServiceWorkerError::GetException(resolver, error, error_msg));
    return;
  }
  resolver->Resolve(error == mojom::ServiceWorkerErrorType::kNone);
}

void DidEnableNavigationPreload(ScriptPromiseResolver* resolver,
                                mojom::ServiceWorkerErrorType error,
                                const String& error_msg) {
  if (!resolver->GetExecutionContext() ||
      resolver->GetExecutionContext()->IsContextDestroyed()) {
    return;
  }

  if (error != mojom::ServiceWorkerErrorType::kNone) {
    DCHECK(!error_msg.IsNull());
    resolver->Reject(
        ServiceWorkerError::GetException(resolver, error, error_msg));
    return;
  }
  resolver->Resolve();
}

void DidGetNavigationPreloadState(
    ScriptPromiseResolver* resolver,
    mojom::ServiceWorkerErrorType error,
    const String& error_msg,
    mojom::blink::NavigationPreloadStatePtr state) {
  if (!resolver->GetExecutionContext() ||
      resolver->GetExecutionContext()->IsContextDestroyed()) {
    return;
  }

  if (error != mojom::ServiceWorkerErrorType::kNone) {
    DCHECK(!error_msg.IsNull());
    resolver->Reject(
        ServiceWorkerError::GetException(resolver, error, error_msg));
    return;
  }
  NavigationPreloadState* dict = NavigationPreloadState::Create();
  dict->setEnabled(state->enabled);
  dict->setHeaderValue(state->header);
  resolver->Resolve(dict);
}

void DidSetNavigationPreloadHeader(ScriptPromiseResolver* resolver,
                                   mojom::ServiceWorkerErrorType error,
                                   const String& error_msg) {
  if (!resolver->GetExecutionContext() ||
      resolver->GetExecutionContext()->IsContextDestroyed()) {
    return;
  }

  if (error != mojom::ServiceWorkerErrorType::kNone) {
    DCHECK(!error_msg.IsNull());
    resolver->Reject(
        ServiceWorkerError::GetException(resolver, error, error_msg));
    return;
  }
  resolver->Resolve();
}

}  // namespace

ServiceWorkerRegistration* ServiceWorkerRegistration::Take(
    ScriptPromiseResolver* resolver,
    WebServiceWorkerRegistrationObjectInfo info) {
  return ServiceWorkerContainer::From(
             To<Document>(resolver->GetExecutionContext()))
      ->GetOrCreateServiceWorkerRegistration(std::move(info));
}

ServiceWorkerRegistration::ServiceWorkerRegistration(
    ExecutionContext* execution_context,
    WebServiceWorkerRegistrationObjectInfo info)
    : ContextLifecycleObserver(execution_context),
      registration_id_(info.registration_id),
      scope_(std::move(info.scope)),
      stopped_(false) {
  DCHECK_NE(mojom::blink::kInvalidServiceWorkerRegistrationId,
            registration_id_);
  Attach(std::move(info));
}

ServiceWorkerRegistration::ServiceWorkerRegistration(
    ExecutionContext* execution_context,
    mojom::blink::ServiceWorkerRegistrationObjectInfoPtr info)
    : ContextLifecycleObserver(execution_context),
      registration_id_(info->registration_id),
      scope_(std::move(info->scope)),
      stopped_(false) {
  DCHECK_NE(mojom::blink::kInvalidServiceWorkerRegistrationId,
            registration_id_);

  host_.Bind(
      std::move(info->host_remote),
      GetExecutionContext()->GetTaskRunner(blink::TaskType::kInternalDefault));
  // The host expects us to use |info.receiver| so bind to it.
  receiver_.Bind(
      std::move(info->receiver),
      GetExecutionContext()->GetTaskRunner(blink::TaskType::kInternalDefault));

  update_via_cache_ = info->update_via_cache;
  installing_ =
      ServiceWorker::From(GetExecutionContext(), std::move(info->installing));
  waiting_ =
      ServiceWorker::From(GetExecutionContext(), std::move(info->waiting));
  active_ = ServiceWorker::From(GetExecutionContext(), std::move(info->active));
}

void ServiceWorkerRegistration::Attach(
    WebServiceWorkerRegistrationObjectInfo info) {
  DCHECK_EQ(registration_id_, info.registration_id);
  DCHECK_EQ(scope_.GetString(), WTF::String(info.scope.GetString()));

  // If |host_| is bound, it already points to the same object host as
  // |info.host_remote|, so there is no need to bind again.
  if (!host_) {
    host_.Bind(mojo::PendingAssociatedRemote<
                   mojom::blink::ServiceWorkerRegistrationObjectHost>(
                   std::move(info.host_remote),
                   mojom::blink::ServiceWorkerRegistrationObjectHost::Version_),
               GetExecutionContext()->GetTaskRunner(
                   blink::TaskType::kInternalDefault));
  }
  // The host expects us to use |info.receiver| so bind to it.
  receiver_.reset();
  receiver_.Bind(
      mojo::PendingAssociatedReceiver<
          mojom::blink::ServiceWorkerRegistrationObject>(
          std::move(info.receiver)),
      GetExecutionContext()->GetTaskRunner(blink::TaskType::kInternalDefault));

  update_via_cache_ = info.update_via_cache;
  installing_ =
      ServiceWorker::From(GetExecutionContext(), std::move(info.installing));
  waiting_ =
      ServiceWorker::From(GetExecutionContext(), std::move(info.waiting));
  active_ = ServiceWorker::From(GetExecutionContext(), std::move(info.active));
}

bool ServiceWorkerRegistration::HasPendingActivity() const {
  return !stopped_;
}

const AtomicString& ServiceWorkerRegistration::InterfaceName() const {
  return event_target_names::kServiceWorkerRegistration;
}

NavigationPreloadManager* ServiceWorkerRegistration::navigationPreload() {
  if (!navigation_preload_)
    navigation_preload_ = NavigationPreloadManager::Create(this);
  return navigation_preload_;
}

String ServiceWorkerRegistration::scope() const {
  return scope_.GetString();
}

String ServiceWorkerRegistration::updateViaCache() const {
  switch (update_via_cache_) {
    case mojom::ServiceWorkerUpdateViaCache::kImports:
      return "imports";
    case mojom::ServiceWorkerUpdateViaCache::kAll:
      return "all";
    case mojom::ServiceWorkerUpdateViaCache::kNone:
      return "none";
  }
  NOTREACHED();
  return "";
}

void ServiceWorkerRegistration::EnableNavigationPreload(
    bool enable,
    ScriptPromiseResolver* resolver) {
  host_->EnableNavigationPreload(
      enable, WTF::Bind(&DidEnableNavigationPreload, WrapPersistent(resolver)));
}

void ServiceWorkerRegistration::GetNavigationPreloadState(
    ScriptPromiseResolver* resolver) {
  host_->GetNavigationPreloadState(
      WTF::Bind(&DidGetNavigationPreloadState, WrapPersistent(resolver)));
}

void ServiceWorkerRegistration::SetNavigationPreloadHeader(
    const String& value,
    ScriptPromiseResolver* resolver) {
  host_->SetNavigationPreloadHeader(
      value,
      WTF::Bind(&DidSetNavigationPreloadHeader, WrapPersistent(resolver)));
}

ScriptPromise ServiceWorkerRegistration::update(ScriptState* script_state) {
  if (!GetExecutionContext()) {
    return ScriptPromise::RejectWithDOMException(
        script_state, MakeGarbageCollected<DOMException>(
                          DOMExceptionCode::kInvalidStateError,
                          "Failed to update a ServiceWorkerRegistration: No "
                          "associated provider is available."));
  }

  // The fetcher is lazily loaded in a worker global scope.
  auto* execution_context = ExecutionContext::From(script_state);
  if (auto* global_scope = DynamicTo<WorkerGlobalScope>(execution_context))
    global_scope->EnsureFetcher();

  const FetchClientSettingsObject& settings_object =
      execution_context->Fetcher()
          ->GetProperties()
          .GetFetchClientSettingsObject();
  auto mojom_settings_object = mojom::blink::FetchClientSettingsObject::New(
      settings_object.GetReferrerPolicy(),
      KURL(settings_object.GetOutgoingReferrer()),
      settings_object.GetInsecureRequestsPolicy() &
              blink::kUpgradeInsecureRequests
          ? blink::mojom::InsecureRequestsPolicy::kUpgrade
          : blink::mojom::InsecureRequestsPolicy::kDoNotUpgrade);

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  host_->Update(
      std::move(mojom_settings_object),
      WTF::Bind(&DidUpdate, WrapPersistent(resolver), WrapPersistent(this)));
  return resolver->Promise();
}

ScriptPromise ServiceWorkerRegistration::unregister(ScriptState* script_state) {
  if (!GetExecutionContext()) {
    return ScriptPromise::RejectWithDOMException(
        script_state, MakeGarbageCollected<DOMException>(
                          DOMExceptionCode::kInvalidStateError,
                          "Failed to unregister a "
                          "ServiceWorkerRegistration: No "
                          "associated provider is available."));
  }
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  host_->Unregister(WTF::Bind(&DidUnregister, WrapPersistent(resolver)));
  return resolver->Promise();
}

ServiceWorkerRegistration::~ServiceWorkerRegistration() = default;

void ServiceWorkerRegistration::Dispose() {
  host_.reset();
  receiver_.reset();
}

void ServiceWorkerRegistration::Trace(blink::Visitor* visitor) {
  visitor->Trace(installing_);
  visitor->Trace(waiting_);
  visitor->Trace(active_);
  visitor->Trace(navigation_preload_);
  EventTargetWithInlineData::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
  Supplementable<ServiceWorkerRegistration>::Trace(visitor);
}

void ServiceWorkerRegistration::ContextDestroyed(ExecutionContext*) {
  if (stopped_)
    return;
  stopped_ = true;
}

void ServiceWorkerRegistration::SetServiceWorkerObjects(
    mojom::blink::ChangedServiceWorkerObjectsMaskPtr changed_mask,
    mojom::blink::ServiceWorkerObjectInfoPtr installing,
    mojom::blink::ServiceWorkerObjectInfoPtr waiting,
    mojom::blink::ServiceWorkerObjectInfoPtr active) {
  if (!GetExecutionContext())
    return;

  DCHECK(changed_mask->installing || !installing);
  if (changed_mask->installing) {
    installing_ =
        ServiceWorker::From(GetExecutionContext(), std::move(installing));
  }
  DCHECK(changed_mask->waiting || !waiting);
  if (changed_mask->waiting) {
    waiting_ = ServiceWorker::From(GetExecutionContext(), std::move(waiting));
  }
  DCHECK(changed_mask->active || !active);
  if (changed_mask->active) {
    active_ = ServiceWorker::From(GetExecutionContext(), std::move(active));
  }
}

void ServiceWorkerRegistration::SetUpdateViaCache(
    mojom::blink::ServiceWorkerUpdateViaCache update_via_cache) {
  update_via_cache_ = update_via_cache;
}

void ServiceWorkerRegistration::UpdateFound() {
  DispatchEvent(*Event::Create(event_type_names::kUpdatefound));
}

}  // namespace blink

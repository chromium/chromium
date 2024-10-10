// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/service_worker/service_worker_registration.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "third_party/blink/public/common/security_context/insecure_request_policy.h"
#include "third_party/blink/public/mojom/loader/fetch_client_settings_object.mojom-blink.h"
#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom-blink.h"
#include "third_party/blink/public/mojom/service_worker/navigation_preload_state.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_navigation_preload_state.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_service_worker_update_via_cache.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_container.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_error.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher_properties.h"

namespace blink {

namespace {

void DidUpdate(ScriptPromiseResolver<ServiceWorkerRegistration>* resolver,
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

void DidUnregister(ScriptPromiseResolver<IDLBoolean>* resolver,
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

void DidEnableNavigationPreload(ScriptPromiseResolver<IDLUndefined>* resolver,
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
    ScriptPromiseResolver<NavigationPreloadState>* resolver,
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

void DidSetNavigationPreloadHeader(
    ScriptPromiseResolver<IDLUndefined>* resolver,
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
    ScriptPromiseResolverBase* resolver,
    WebServiceWorkerRegistrationObjectInfo info) {
  return ServiceWorkerContainer::From(
             *To<LocalDOMWindow>(resolver->GetExecutionContext()))
      ->GetOrCreateServiceWorkerRegistration(std::move(info));
}

ServiceWorkerRegistration::ServiceWorkerRegistration(
    ExecutionContext* execution_context,
    WebServiceWorkerRegistrationObjectInfo info)
    : ActiveScriptWrappable<ServiceWorkerRegistration>({}),
      ExecutionContextLifecycleObserver(execution_context),
      registration_id_(info.registration_id),
      scope_(std::move(info.scope)),
      host_(execution_context),
      receiver_(this, execution_context),
      stopped_(false) {
  DCHECK_NE(mojom::blink::kInvalidServiceWorkerRegistrationId,
            registration_id_);
  Attach(std::move(info));
}

ServiceWorkerRegistration::ServiceWorkerRegistration(
    ExecutionContext* execution_context,
    mojom::blink::ServiceWorkerRegistrationObjectInfoPtr info)
    : ActiveScriptWrappable<ServiceWorkerRegistration>({}),
      ExecutionContextLifecycleObserver(execution_context),
      registration_id_(info->registration_id),
      scope_(std::move(info->scope)),
      host_(execution_context),
      receiver_(this, execution_context),
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
  if (!host_.is_bound()) {
    host_.Bind(std::move(info.host_remote),
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
    navigation_preload_ = MakeGarbageCollected<NavigationPreloadManager>(this);
  return navigation_preload_.Get();
}

String ServiceWorkerRegistration::scope() const {
  return scope_.GetString();
}

V8ServiceWorkerUpdateViaCache ServiceWorkerRegistration::updateViaCache()
    const {
  switch (update_via_cache_) {
    case mojom::ServiceWorkerUpdateViaCache::kImports:
      return V8ServiceWorkerUpdateViaCache(
          V8ServiceWorkerUpdateViaCache::Enum::kImports);
    case mojom::ServiceWorkerUpdateViaCache::kAll:
      return V8ServiceWorkerUpdateViaCache(
          V8ServiceWorkerUpdateViaCache::Enum::kAll);
    case mojom::ServiceWorkerUpdateViaCache::kNone:
      return V8ServiceWorkerUpdateViaCache(
          V8ServiceWorkerUpdateViaCache::Enum::kNone);
  }
  NOTREACHED();
}

void ServiceWorkerRegistration::EnableNavigationPreload(
    bool enable,
    ScriptPromiseResolver<IDLUndefined>* resolver) {
  if (!host_.is_bound()) {
    return;
  }
  host_->EnableNavigationPreload(
      enable,
      WTF::BindOnce(&DidEnableNavigationPreload, WrapPersistent(resolver)));
}

void ServiceWorkerRegistration::GetNavigationPreloadState(
    ScriptPromiseResolver<NavigationPreloadState>* resolver) {
  if (!host_.is_bound()) {
    return;
  }
  host_->GetNavigationPreloadState(
      WTF::BindOnce(&DidGetNavigationPreloadState, WrapPersistent(resolver)));
}

void ServiceWorkerRegistration::SetNavigationPreloadHeader(
    const String& value,
    ScriptPromiseResolver<IDLUndefined>* resolver) {
  if (!host_.is_bound()) {
    return;
  }
  host_->SetNavigationPreloadHeader(
      value,
      WTF::BindOnce(&DidSetNavigationPreloadHeader, WrapPersistent(resolver)));
}

ScriptPromise<ServiceWorkerRegistration> ServiceWorkerRegistration::update(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!GetExecutionContext()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Failed to update a ServiceWorkerRegistration: No associated provider "
        "is available.");
    return EmptyPromise();
  }

  auto* execution_context = ExecutionContext::From(script_state);

  const FetchClientSettingsObject& settings_object =
      execution_context->Fetcher()
          ->GetProperties()
          .GetFetchClientSettingsObject();
  auto mojom_settings_object = mojom::blink::FetchClientSettingsObject::New(
      settings_object.GetReferrerPolicy(),
      KURL(settings_object.GetOutgoingReferrer()),
      (settings_object.GetInsecureRequestsPolicy() &
       mojom::blink::InsecureRequestPolicy::kUpgradeInsecureRequests) !=
              mojom::blink::InsecureRequestPolicy::kLeaveInsecureRequestsAlone
          ? blink::mojom::InsecureRequestsPolicy::kUpgrade
          : blink::mojom::InsecureRequestsPolicy::kDoNotUpgrade);

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<ServiceWorkerRegistration>>(
          script_state);

  // Defer update() from a prerendered page until page activation.
  // https://wicg.github.io/nav-speculation/prerendering.html#patch-service-workers
  if (GetExecutionContext()->IsWindow()) {
    Document* document = To<LocalDOMWindow>(GetExecutionContext())->document();
    if (document->IsPrerendering()) {
      document->AddPostPrerenderingActivationStep(WTF::BindOnce(
          &ServiceWorkerRegistration::UpdateInternal, WrapWeakPersistent(this),
          std::move(mojom_settings_object), WrapPersistent(resolver)));
      return resolver->Promise();
    }
  }

  UpdateInternal(std::move(mojom_settings_object), resolver);
  return resolver->Promise();
}

ScriptPromise<IDLBoolean> ServiceWorkerRegistration::unregister(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!GetExecutionContext()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Failed to unregister a "
                                      "ServiceWorkerRegistration: No "
                                      "associated provider is available.");
    return EmptyPromise();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLBoolean>>(script_state);

  // Defer unregister() from a prerendered page until page activation.
  // https://wicg.github.io/nav-speculation/prerendering.html#patch-service-workers
  if (GetExecutionContext()->IsWindow()) {
    Document* document = To<LocalDOMWindow>(GetExecutionContext())->document();
    if (document->IsPrerendering()) {
      document->AddPostPrerenderingActivationStep(
          WTF::BindOnce(&ServiceWorkerRegistration::UnregisterInternal,
                        WrapWeakPersistent(this), WrapPersistent(resolver)));
      return resolver->Promise();
    }
  }

  UnregisterInternal(resolver);
  return resolver->Promise();
}

ServiceWorkerRegistration::~ServiceWorkerRegistration() = default;

void ServiceWorkerRegistration::Dispose() {
  host_.reset();
  receiver_.reset();
}

void ServiceWorkerRegistration::Trace(Visitor* visitor) const {
  visitor->Trace(installing_);
  visitor->Trace(waiting_);
  visitor->Trace(active_);
  visitor->Trace(navigation_preload_);
  visitor->Trace(host_);
  visitor->Trace(receiver_);
  EventTarget::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
  Supplementable<ServiceWorkerRegistration>::Trace(visitor);
}

void ServiceWorkerRegistration::ContextDestroyed() {
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

void ServiceWorkerRegistration::UpdateInternal(
    mojom::blink::FetchClientSettingsObjectPtr mojom_settings_object,
    ScriptPromiseResolver<ServiceWorkerRegistration>* resolver) {
  if (!host_.is_bound()) {
    return;
  }
  host_->Update(std::move(mojom_settings_object),
                WTF::BindOnce(&DidUpdate, WrapPersistent(resolver),
                              WrapPersistent(this)));
}

void ServiceWorkerRegistration::UnregisterInternal(
    ScriptPromiseResolver<IDLBoolean>* resolver) {
  if (!host_.is_bound()) {
    return;
  }
  host_->Unregister(WTF::BindOnce(&DidUnregister, WrapPersistent(resolver)));
}

}  // namespace blink

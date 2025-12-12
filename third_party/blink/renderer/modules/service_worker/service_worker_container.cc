/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "third_party/blink/renderer/modules/service_worker/service_worker_container.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_error_type.mojom-blink.h"
#include "third_party/blink/public/platform/web_callbacks.h"
#include "third_party/blink/public/platform/web_fetch_client_settings_object.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value_factory.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_trustedscripturl_usvstring.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/events/message_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/deprecation/deprecation.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/messaging/blink_transferable_message.h"
#include "third_party/blink/renderer/core/messaging/message_port.h"
#include "third_party/blink/renderer/core/script/script.h"
#include "third_party/blink/renderer/core/script_type_names.h"
#include "third_party/blink/renderer/core/workers/dedicated_worker_global_scope.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_error.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_registration.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/weborigin/reporting_disposition.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

template <typename WebType>
struct WebTypeTraits;

template <>
struct WebTypeTraits<WebServiceWorkerRegistrationObjectInfo> {
  using IDLType = ServiceWorkerRegistration*;
  static ServiceWorkerRegistration* ToIDLType(
      ScriptState* script_state,
      WebServiceWorkerRegistrationObjectInfo info) {
    return ServiceWorkerContainer::From(*ExecutionContext::From(script_state))
        ->GetOrCreateServiceWorkerRegistration(std::move(info));
  }
};

template <>
struct WebTypeTraits<std::vector<WebServiceWorkerRegistrationObjectInfo>> {
  using IDLType = IDLSequence<ServiceWorkerRegistration>;
  static HeapVector<Member<ServiceWorkerRegistration>> ToIDLType(
      ScriptState* script_state,
      std::vector<WebServiceWorkerRegistrationObjectInfo> infos) {
    HeapVector<Member<ServiceWorkerRegistration>> registrations;
    for (auto& info : infos) {
      registrations.push_back(
          WebTypeTraits<WebServiceWorkerRegistrationObjectInfo>::ToIDLType(
              script_state, std::move(info)));
    }
    return registrations;
  }
};

template <>
struct WebTypeTraits<WebServiceWorkerError> {
  using IDLType = DOMException*;
  static DOMException* ToIDLType(ScriptState*,
                                 const WebServiceWorkerError& error) {
    return ServiceWorkerError::AsException(error.error_type, error.message);
  }
};

struct WebServiceWorkerErrorTraitsForUpdate {
  using IDLType = IDLAny;
  static v8::Local<v8::Value> ToIDLType(ScriptState* script_state,
                                        const WebServiceWorkerError& error) {
    return ServiceWorkerErrorForUpdate::AsJSException(
        script_state, error.error_type, error.message);
  }
};

template <typename WebSuccessResult,
          typename WebFailureResult,
          typename FailureTraits =
              WebTypeTraits<std::remove_cvref_t<WebFailureResult>>>
class CallbackPromiseAdapter
    : public WebCallbacks<WebSuccessResult, WebFailureResult> {
  using IDLResolveType = typename WebTypeTraits<WebSuccessResult>::IDLType;
  using ResolverType =
      ScriptPromiseResolver<std::remove_pointer_t<IDLResolveType>>;

 public:
  explicit CallbackPromiseAdapter(ResolverType* resolver)
      : resolver_(resolver) {}
  ~CallbackPromiseAdapter() override = default;

 private:
  void OnSuccess(WebSuccessResult result) override {
    ScriptState* script_state = resolver_->GetScriptState();
    if (!script_state->ContextIsValid()) {
      return;
    }
    resolver_->Resolve(WebTypeTraits<WebSuccessResult>::ToIDLType(
        script_state, std::move(result)));
  }

  void OnError(WebFailureResult result) override {
    ScriptState* script_state = resolver_->GetScriptState();
    if (!script_state->ContextIsValid()) {
      return;
    }
    ScriptState::Scope scope(script_state);
    resolver_->Reject(
        FailureTraits::ToIDLType(script_state, std::move(result)));
  }

  Persistent<ResolverType> const resolver_;
};

void MaybeRecordThirdPartyServiceWorkerUsage(
    ExecutionContext* execution_context) {
  DCHECK(execution_context);
  if (execution_context->IsWindow()) {
    LocalDOMWindow* window = To<LocalDOMWindow>(execution_context);
    DCHECK(window);

    if (window->IsCrossSiteSubframe()) {
      UseCounter::Count(window, WebFeature::kThirdPartyServiceWorker);
    }
  }
}

bool HasFiredDomContentLoaded(const Document& document) {
  return !document.GetTiming().DomContentLoadedEventStart().is_null();
}

mojom::blink::ServiceWorkerUpdateViaCache V8EnumToUpdateViaCache(
    V8ServiceWorkerUpdateViaCache::Enum value) {
  switch (value) {
    case V8ServiceWorkerUpdateViaCache::Enum::kImports:
      return mojom::blink::ServiceWorkerUpdateViaCache::kImports;
    case V8ServiceWorkerUpdateViaCache::Enum::kAll:
      return mojom::blink::ServiceWorkerUpdateViaCache::kAll;
    case V8ServiceWorkerUpdateViaCache::Enum::kNone:
      return mojom::blink::ServiceWorkerUpdateViaCache::kNone;
  }
  NOTREACHED();
}

// TODO(caseq): reuse CallbackPromiseAdapter.
class GetRegistrationCallback : public WebServiceWorkerProvider::
                                    WebServiceWorkerGetRegistrationCallbacks {
 public:
  explicit GetRegistrationCallback(
      ScriptPromiseResolver<ServiceWorkerRegistration>* resolver)
      : resolver_(resolver) {}

  GetRegistrationCallback(const GetRegistrationCallback&) = delete;
  GetRegistrationCallback& operator=(const GetRegistrationCallback&) = delete;

  ~GetRegistrationCallback() override = default;

  void OnSuccess(WebServiceWorkerRegistrationObjectInfo info) override {
    if (!resolver_->GetExecutionContext() ||
        resolver_->GetExecutionContext()->IsContextDestroyed())
      return;
    if (info.registration_id ==
        mojom::blink::kInvalidServiceWorkerRegistrationId) {
      // Resolve the promise with undefined.
      resolver_->Resolve();
      return;
    }
    resolver_->Resolve(
        ServiceWorkerContainer::From(*resolver_->GetExecutionContext())
            ->GetOrCreateServiceWorkerRegistration(std::move(info)));
  }

  void OnError(const WebServiceWorkerError& error) override {
    if (!resolver_->GetExecutionContext() ||
        resolver_->GetExecutionContext()->IsContextDestroyed())
      return;
    resolver_->Reject(
        ServiceWorkerError::AsException(error.error_type, error.message));
  }

 private:
  Persistent<ScriptPromiseResolver<ServiceWorkerRegistration>> resolver_;
};

}  // namespace

class ServiceWorkerContainer::DomContentLoadedListener final
    : public NativeEventListener {
 public:
  void Invoke(ExecutionContext* execution_context, Event* event) override {
    DCHECK_EQ(event->type(), "DOMContentLoaded");

    // We can only get DOMContentLoaded event from a Window, not a Worker.
    DCHECK(execution_context->IsWindow());
    LocalDOMWindow& window = *To<LocalDOMWindow>(execution_context);
    DCHECK(HasFiredDomContentLoaded(*window.document()));

    auto* container =
        Supplement<ExecutionContext>::From<ServiceWorkerContainer>(
            execution_context);
    if (!container) {
      // There is no container for some reason, which means there's no message
      // queue to start. Just abort.
      return;
    }

    container->EnableClientMessageQueue();
  }
};

const char ServiceWorkerContainer::kSupplementName[] = "ServiceWorkerContainer";

ServiceWorkerContainer* ServiceWorkerContainer::From(
    ExecutionContext& execution_context) {
  ServiceWorkerContainer* container =
      Supplement<ExecutionContext>::From<ServiceWorkerContainer>(
          execution_context);
  if (!container) {
    // TODO(leonhsl): Figure out whether it's really necessary to create an
    // instance when there's no frame or frame client for |window|.
    container = MakeGarbageCollected<ServiceWorkerContainer>(execution_context);
    Supplement<ExecutionContext>::ProvideTo(execution_context, container);
    std::unique_ptr<WebServiceWorkerProvider> provider;

    if (execution_context.IsWindow()) {
      auto& window = To<LocalDOMWindow>(execution_context);
      if (window.GetFrame() && window.GetFrame()->Client()) {
        provider = window.GetFrame()->Client()->CreateServiceWorkerProvider();
      }
    } else if (execution_context.IsDedicatedWorkerGlobalScope()) {
      CHECK(base::FeatureList::IsEnabled(
          blink::features::kServiceWorkerInDedicatedWorker));
      auto& worker = To<DedicatedWorkerGlobalScope>(execution_context);
      provider = worker.CreateServiceWorkerProvider();
    } else {
      // TODO(https://crbug.com/422940475): Add support for Service Worker
      // APIs in shared workers.
      NOTREACHED() << "ServiceWorkerContainer can only be created for a "
                      "Window or DedicatedWorkerGlobalScope.";
    }

    if (provider) {
      provider->SetClient(container);
      container->provider_ = std::move(provider);
    }
  }
  return container;
}

ServiceWorkerContainer* ServiceWorkerContainer::CreateForTesting(
    ExecutionContext& execution_context,
    std::unique_ptr<WebServiceWorkerProvider> provider) {
  ServiceWorkerContainer* container =
      MakeGarbageCollected<ServiceWorkerContainer>(execution_context);
  container->provider_ = std::move(provider);
  return container;
}

ServiceWorkerContainer::~ServiceWorkerContainer() {
  DCHECK(!provider_);
}

void ServiceWorkerContainer::ContextDestroyed() {
  if (provider_) {
    provider_->SetClient(nullptr);
    provider_ = nullptr;
  }
  controller_ = nullptr;
}

void ServiceWorkerContainer::Trace(Visitor* visitor) const {
  visitor->Trace(controller_);
  visitor->Trace(ready_);
  visitor->Trace(dom_content_loaded_observer_);
  visitor->Trace(service_worker_registration_objects_);
  visitor->Trace(service_worker_objects_);
  EventTarget::Trace(visitor);
  Supplement<ExecutionContext>::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

ScriptPromise<ServiceWorkerRegistration>
ServiceWorkerContainer::registerServiceWorker(
    ScriptState* script_state,
    const V8UnionTrustedScriptURLOrUSVString* untrusted_url,
    const RegistrationOptions* options,
    ExceptionState& exception_state) {
  // step 2 of
  // https://w3c.github.io/ServiceWorker/#dom-serviceworkercontainer-register
  String url = TrustedTypesCheckForScriptURL(
      untrusted_url, GetExecutionContext(),
      trusted_types_names::kServiceWorkerContainer,
      trusted_types_names::kRegister, exception_state);
  if (exception_state.HadException()) {
    return {};
  }

  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The document is in an invalid state.");
    return {};
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<ServiceWorkerRegistration>>(
          script_state);
  auto promise = resolver->Promise();

  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  MaybeRecordThirdPartyServiceWorkerUsage(execution_context);

  // The IDL definition is expected to restrict service worker to secure
  // contexts.
  CHECK(execution_context->IsSecureContext());

  scoped_refptr<const SecurityOrigin> document_origin =
      execution_context->GetSecurityOrigin();
  KURL page_url = KURL(NullURL(), document_origin->ToString());
  if (!SchemeRegistry::ShouldTreatURLSchemeAsAllowingServiceWorkers(
          page_url.Protocol())) {
    resolver->Reject(ServiceWorkerErrorForUpdate::AsJSException(
        script_state, mojom::blink::ServiceWorkerErrorType::kType,
        StrCat({"Failed to register a ServiceWorker: The URL protocol of the "
                "current origin ('",
                document_origin->ToString(), "') is not supported."})));
    return promise;
  }

  KURL script_url = execution_context->CompleteURL(url);
  script_url.RemoveFragmentIdentifier();

  if (!SchemeRegistry::ShouldTreatURLSchemeAsAllowingServiceWorkers(
          script_url.Protocol())) {
    resolver->Reject(ServiceWorkerErrorForUpdate::AsJSException(
        script_state, mojom::blink::ServiceWorkerErrorType::kType,
        StrCat({"Failed to register a ServiceWorker: The URL protocol of the "
                "script ('",
                script_url.GetString(), "') is not supported."})));
    return promise;
  }

  if (!document_origin->CanRequest(script_url)) {
    scoped_refptr<const SecurityOrigin> script_origin =
        SecurityOrigin::Create(script_url);
    resolver->Reject(ServiceWorkerErrorForUpdate::AsJSException(
        script_state, mojom::blink::ServiceWorkerErrorType::kSecurity,
        StrCat({"Failed to register a ServiceWorker: The "
                "origin of the provided scriptURL ('",
                script_origin->ToString(),
                "') does not match the current origin ('",
                document_origin->ToString(), "')."})));
    return promise;
  }

  KURL scope_url;
  if (options->hasScope())
    scope_url = execution_context->CompleteURL(options->scope());
  else
    scope_url = KURL(script_url, "./");
  scope_url.RemoveFragmentIdentifier();

  if (!SchemeRegistry::ShouldTreatURLSchemeAsAllowingServiceWorkers(
          scope_url.Protocol())) {
    resolver->Reject(ServiceWorkerErrorForUpdate::AsJSException(
        script_state, mojom::blink::ServiceWorkerErrorType::kType,
        StrCat({"Failed to register a ServiceWorker: The URL protocol of the "
                "scope ('",
                scope_url.GetString(), "') is not supported."})));
    return promise;
  }

  if (!document_origin->CanRequest(scope_url)) {
    scoped_refptr<const SecurityOrigin> scope_origin =
        SecurityOrigin::Create(scope_url);
    resolver->Reject(ServiceWorkerErrorForUpdate::AsJSException(
        script_state, mojom::blink::ServiceWorkerErrorType::kSecurity,
        StrCat({"Failed to register a ServiceWorker: The origin of the "
                "provided scope ('",
                scope_origin->ToString(),
                "') does not match the current origin ('",
                document_origin->ToString(), "')."})));
    return promise;
  }

  if (!provider_) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError,
        "Failed to register a ServiceWorker: "
        "The document is in an invalid state."));
    return promise;
  }

  WebString web_error_message;
  if (!provider_->ValidateScopeAndScriptURL(scope_url, script_url,
                                            &web_error_message)) {
    resolver->Reject(ServiceWorkerErrorForUpdate::AsJSException(
        script_state, mojom::blink::ServiceWorkerErrorType::kType,
        StrCat({"Failed to register a ServiceWorker: ", web_error_message})));
    return promise;
  }

  ContentSecurityPolicy* csp = execution_context->GetContentSecurityPolicy();
  if (csp && !csp->AllowWorkerContextFromSource(script_url)) {
    resolver->Reject(ServiceWorkerErrorForUpdate::AsJSException(
        script_state, mojom::blink::ServiceWorkerErrorType::kSecurity,
        StrCat({"Failed to register a ServiceWorker: The provided scriptURL ('",
                script_url.GetString(),
                "') violates the Content Security Policy."})));
    return promise;
  }

  mojom::blink::ServiceWorkerUpdateViaCache update_via_cache =
      V8EnumToUpdateViaCache(options->updateViaCache().AsEnum());
  mojom::blink::ScriptType script_type =
      Script::V8WorkerTypeToScriptType(options->type().AsEnum());

  WebFetchClientSettingsObject fetch_client_settings_object(
      execution_context->Fetcher()
          ->GetProperties()
          .GetFetchClientSettingsObject());

  auto callbacks = std::make_unique<CallbackPromiseAdapter<
      WebServiceWorkerRegistrationObjectInfo, const WebServiceWorkerError&,
      WebServiceWorkerErrorTraitsForUpdate>>(resolver);

  // Defer register() from a prerendered page until page activation.
  // https://wicg.github.io/nav-speculation/prerendering.html#patch-service-workers
  if (GetExecutionContext()->IsWindow()) {
    Document* document = To<LocalDOMWindow>(GetExecutionContext())->document();
    if (document->IsPrerendering()) {
      document->AddPostPrerenderingActivationStep(BindOnce(
          &ServiceWorkerContainer::RegisterServiceWorkerInternal,
          WrapWeakPersistent(this), scope_url, script_url,
          std::move(script_type), update_via_cache,
          std::move(fetch_client_settings_object), std::move(callbacks)));
      return promise;
    }
  }

  RegisterServiceWorkerInternal(
      scope_url, script_url, std::move(script_type), update_via_cache,
      std::move(fetch_client_settings_object), std::move(callbacks));
  return promise;
}

void ServiceWorkerContainer::RegisterServiceWorkerInternal(
    const KURL& scope_url,
    const KURL& script_url,
    std::optional<mojom::blink::ScriptType> script_type,
    mojom::blink::ServiceWorkerUpdateViaCache update_via_cache,
    WebFetchClientSettingsObject fetch_client_settings_object,
    std::unique_ptr<RegistrationCallbacks> callbacks) {
  if (!provider_)
    return;
  provider_->RegisterServiceWorker(
      scope_url, script_url, *script_type, update_via_cache,
      std::move(fetch_client_settings_object), std::move(callbacks));
}

ScriptPromise<ServiceWorkerRegistration>
ServiceWorkerContainer::getRegistration(ScriptState* script_state,
                                        const String& document_url) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<ServiceWorkerRegistration>>(
          script_state);
  auto promise = resolver->Promise();

  ExecutionContext* execution_context = ExecutionContext::From(script_state);

  // The IDL definition is expected to restrict service worker to secure
  // contexts.
  CHECK(execution_context->IsSecureContext());

  scoped_refptr<const SecurityOrigin> document_origin =
      execution_context->GetSecurityOrigin();
  KURL page_url = KURL(NullURL(), document_origin->ToString());
  if (!SchemeRegistry::ShouldTreatURLSchemeAsAllowingServiceWorkers(
          page_url.Protocol())) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kSecurityError,
        StrCat({"Failed to get a ServiceWorkerRegistration: The URL protocol "
                "of the current origin ('",
                document_origin->ToString(), "') is not supported."})));
    return promise;
  }

  KURL completed_url = execution_context->CompleteURL(document_url);
  completed_url.RemoveFragmentIdentifier();
  if (!document_origin->CanRequest(completed_url)) {
    scoped_refptr<const SecurityOrigin> document_url_origin =
        SecurityOrigin::Create(completed_url);
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kSecurityError,
        StrCat({"Failed to get a ServiceWorkerRegistration: The origin of the "
                "provided documentURL ('",
                document_url_origin->ToString(),
                "') does not match the current origin ('",
                document_origin->ToString(), "')."})));
    return promise;
  }

  if (!provider_) {
    resolver->Reject(
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kInvalidStateError,
                                           "Failed to get a "
                                           "ServiceWorkerRegistration: The "
                                           "document is in an invalid state."));
    return promise;
  }
  provider_->GetRegistration(
      completed_url, std::make_unique<GetRegistrationCallback>(resolver));

  return promise;
}

ScriptPromise<IDLSequence<ServiceWorkerRegistration>>
ServiceWorkerContainer::getRegistrations(ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<
      ScriptPromiseResolver<IDLSequence<ServiceWorkerRegistration>>>(
      script_state);
  auto promise = resolver->Promise();

  if (!provider_) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError,
        "Failed to get ServiceWorkerRegistration objects: "
        "The document is in an invalid state."));
    return promise;
  }

  ExecutionContext* execution_context = ExecutionContext::From(script_state);

  // The IDL definition is expected to restrict service worker to secure
  // contexts.
  CHECK(execution_context->IsSecureContext());

  scoped_refptr<const SecurityOrigin> document_origin =
      execution_context->GetSecurityOrigin();
  KURL page_url = KURL(NullURL(), document_origin->ToString());
  if (!SchemeRegistry::ShouldTreatURLSchemeAsAllowingServiceWorkers(
          page_url.Protocol())) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kSecurityError,
        StrCat({"Failed to get ServiceWorkerRegistration objects: The URL "
                "protocol of the current origin ('",
                document_origin->ToString(), "') is not supported."})));
    return promise;
  }

  auto callbacks = std::make_unique<CallbackPromiseAdapter<
      std::vector<WebServiceWorkerRegistrationObjectInfo>,
      const WebServiceWorkerError&>>(resolver);

  provider_->GetRegistrations(std::move(callbacks));

  return promise;
}

ScriptPromise<ServiceWorkerRegistration>
ServiceWorkerContainer::registerServiceWorkerWithoutTrustedTypes(
    ScriptState* script_state,
    const String& script_url,
    const RegistrationOptions* options) {
  return registerServiceWorker(
      script_state,
      MakeGarbageCollected<V8UnionTrustedScriptURLOrUSVString>(script_url),
      options, ASSERT_NO_EXCEPTION);
}

// https://w3c.github.io/ServiceWorker/#dom-serviceworkercontainer-startmessages
void ServiceWorkerContainer::startMessages() {
  // "startMessages() method must enable the context object’s client message
  // queue if it is not enabled."
  EnableClientMessageQueue();
}

ScriptPromise<ServiceWorkerRegistration> ServiceWorkerContainer::ready(
    ScriptState* caller_state,
    ExceptionState& exception_state) {
  if (!GetExecutionContext())
    return EmptyPromise();

  if (!caller_state->World().IsMainWorld()) {
    // FIXME: Support .ready from isolated worlds when
    // ScriptPromiseProperty can vend Promises in isolated worlds.
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "'ready' is only supported in pages.");
    return EmptyPromise();
  }

  if (!ready_) {
    ready_ = CreateReadyProperty();
    if (provider_) {
      provider_->GetRegistrationForReady(
          BindOnce(&ServiceWorkerContainer::OnGetRegistrationForReady,
                   WrapPersistent(this)));
    }
  }

  return ready_->Promise(caller_state->World());
}

void ServiceWorkerContainer::SetController(
    WebServiceWorkerObjectInfo info,
    bool should_notify_controller_change) {
  if (!GetExecutionContext())
    return;
  controller_ = ServiceWorker::From(GetExecutionContext(), std::move(info));
  if (controller_) {
    MaybeRecordThirdPartyServiceWorkerUsage(GetExecutionContext());
    UseCounter::Count(GetExecutionContext(),
                      WebFeature::kServiceWorkerControlledPage);
  }
  if (should_notify_controller_change)
    DispatchEvent(*Event::Create(event_type_names::kControllerchange));
}

void ServiceWorkerContainer::ReceiveMessage(WebServiceWorkerObjectInfo source,
                                            TransferableMessage message) {
  if (GetExecutionContext()->IsWindow()) {
    auto* window = DynamicTo<LocalDOMWindow>(GetExecutionContext());
    if (!window) {
      return;
    }
    auto* document = window->document();
    DCHECK(document);

    if (!is_client_message_queue_enabled_) {
      if (!HasFiredDomContentLoaded(*document)) {
        // Wait for DOMContentLoaded. This corresponds to the specification
        // steps for "Parsing HTML documents": "The end" at
        // https://html.spec.whatwg.org/C/#the-end:
        //
        // 1. Fire an event named DOMContentLoaded at the Document object, with
        // its bubbles attribute initialized to true.
        // 2. Enable the client message queue of the ServiceWorkerContainer
        // object whose associated service worker client is the Document
        // object's relevant settings object.
        if (!dom_content_loaded_observer_) {
          dom_content_loaded_observer_ =
              MakeGarbageCollected<DomContentLoadedListener>();
          document->addEventListener(event_type_names::kDOMContentLoaded,
                                     dom_content_loaded_observer_.Get(), false);
        }
        queued_messages_.emplace_back(
            std::make_unique<MessageFromServiceWorker>(std::move(source),
                                                       std::move(message)));
        // The messages will be dispatched once EnableClientMessageQueue() is
        // called.
        return;
      }

      // DOMContentLoaded was fired already, so enable the queue.
      EnableClientMessageQueue();
    }
  }

  DispatchMessageEvent(std::move(source), std::move(message));
}

void ServiceWorkerContainer::CountFeature(mojom::WebFeature feature) {
  if (!GetExecutionContext())
    return;
  if (!Deprecation::IsDeprecated(feature))
    UseCounter::Count(GetExecutionContext(), feature);
  else
    Deprecation::CountDeprecation(GetExecutionContext(), feature);
}

ExecutionContext* ServiceWorkerContainer::GetExecutionContext() const {
  return GetSupplementable();
}

const AtomicString& ServiceWorkerContainer::InterfaceName() const {
  return event_target_names::kServiceWorkerContainer;
}

void ServiceWorkerContainer::setOnmessage(EventListener* listener) {
  SetAttributeEventListener(event_type_names::kMessage, listener);
  // https://w3c.github.io/ServiceWorker/#dom-serviceworkercontainer-onmessage:
  // "The first time the context object’s onmessage IDL attribute is set, its
  // client message queue must be enabled."
  EnableClientMessageQueue();
}

EventListener* ServiceWorkerContainer::onmessage() {
  return GetAttributeEventListener(event_type_names::kMessage);
}

ServiceWorkerRegistration*
ServiceWorkerContainer::GetOrCreateServiceWorkerRegistration(
    WebServiceWorkerRegistrationObjectInfo info) {
  if (info.registration_id == mojom::blink::kInvalidServiceWorkerRegistrationId)
    return nullptr;

  auto it = service_worker_registration_objects_.find(info.registration_id);
  if (it != service_worker_registration_objects_.end()) {
    ServiceWorkerRegistration* registration = it->value;
    registration->Attach(std::move(info));
    return registration;
  }

  const int64_t registration_id = info.registration_id;
  ServiceWorkerRegistration* registration =
      MakeGarbageCollected<ServiceWorkerRegistration>(GetSupplementable(),
                                                      std::move(info));
  service_worker_registration_objects_.Set(registration_id, registration);
  return registration;
}

ServiceWorker* ServiceWorkerContainer::GetOrCreateServiceWorker(
    WebServiceWorkerObjectInfo info) {
  if (info.version_id == mojom::blink::kInvalidServiceWorkerVersionId)
    return nullptr;

  auto it = service_worker_objects_.find(info.version_id);
  if (it != service_worker_objects_.end())
    return it->value.Get();

  const int64_t version_id = info.version_id;
  ServiceWorker* worker =
      ServiceWorker::Create(GetSupplementable(), std::move(info));
  service_worker_objects_.Set(version_id, worker);
  return worker;
}

ServiceWorkerContainer::ServiceWorkerContainer(
    ExecutionContext& execution_context)
    : Supplement<ExecutionContext>(execution_context),
      ExecutionContextLifecycleObserver(&execution_context) {}

ServiceWorkerContainer::ReadyProperty*
ServiceWorkerContainer::CreateReadyProperty() {
  return MakeGarbageCollected<ReadyProperty>(GetExecutionContext());
}

void ServiceWorkerContainer::EnableClientMessageQueue() {
  dom_content_loaded_observer_ = nullptr;
  if (is_client_message_queue_enabled_) {
    DCHECK(queued_messages_.empty());
    return;
  }
  is_client_message_queue_enabled_ = true;
  Vector<std::unique_ptr<MessageFromServiceWorker>> messages;
  messages.swap(queued_messages_);
  for (auto& message : messages) {
    DispatchMessageEvent(std::move(message->source),
                         std::move(message->message));
  }
}

void ServiceWorkerContainer::DispatchMessageEvent(
    WebServiceWorkerObjectInfo source,
    TransferableMessage message) {
  DCHECK(is_client_message_queue_enabled_);

  auto msg =
      BlinkTransferableMessage::FromTransferableMessage(std::move(message));
  GCedMessagePortArray* ports =
      MessagePort::EntanglePorts(*GetExecutionContext(), std::move(msg.ports));
  ServiceWorker* service_worker =
      ServiceWorker::From(GetExecutionContext(), std::move(source));
  Event* event = nullptr;
  // TODO(crbug.com/1018092): Factor out these security checks so they aren't
  // duplicated in so many places.
  if (msg.message->IsOriginCheckRequired()) {
    const SecurityOrigin* target_origin =
        GetExecutionContext()->GetSecurityOrigin();
    if (!msg.sender_origin ||
        !msg.sender_origin->IsSameOriginWith(target_origin)) {
      event = MessageEvent::CreateError(
          GetExecutionContext()->GetSecurityOrigin(), service_worker);
    }
  }
  if (!event) {
    auto* context = GetExecutionContext();
    if ((!msg.locked_to_sender_agent_cluster ||
         context->IsSameAgentCluster(msg.sender_agent_cluster_id)) &&
        msg.message->CanDeserializeIn(context)) {
      event = MessageEvent::Create(ports, std::move(msg.message),
                                   context->GetSecurityOrigin(),
                                   MessageEvent::kMessageIsSameOrigin,
                                   String() /* lastEventId */, service_worker);
    } else {
      event = MessageEvent::CreateError(context->GetSecurityOrigin(),
                                        service_worker);
    }
  }
  // Schedule the event to be dispatched on the correct task source:
  // https://w3c.github.io/ServiceWorker/#dfn-client-message-queue
  EnqueueEvent(*event, TaskType::kServiceWorkerClientMessage);
}

void ServiceWorkerContainer::OnGetRegistrationForReady(
    WebServiceWorkerRegistrationObjectInfo info) {
  DCHECK_EQ(ready_->GetState(), ReadyProperty::kPending);

  ready_->Resolve(GetOrCreateServiceWorkerRegistration(std::move(info)));
}

}  // namespace blink

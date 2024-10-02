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

#include "third_party/blink/public/mojom/service_worker/service_worker_error_type.mojom-blink.h"
#include "third_party/blink/public/platform/web_fetch_client_settings_object.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value_factory.h"
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

void MaybeRecordThirdPartyServiceWorkerUsage(
    ExecutionContext* execution_context) {
  DCHECK(execution_context);
  // ServiceWorkerContainer is only supported on windows.
  LocalDOMWindow* window = To<LocalDOMWindow>(execution_context);
  DCHECK(window);

  if (window->IsCrossSiteSubframe())
    UseCounter::Count(window, WebFeature::kThirdPartyServiceWorker);
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
        ServiceWorkerRegistration::Take(resolver_, std::move(info)));
  }

  void OnError(const WebServiceWorkerError& error) override {
    if (!resolver_->GetExecutionContext() ||
        resolver_->GetExecutionContext()->IsContextDestroyed())
      return;
    resolver_->Reject(ServiceWorkerError::Take(resolver_.Get(), error));
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

    LocalDOMWindow& window = *To<LocalDOMWindow>(execution_context);
    DCHECK(HasFiredDomContentLoaded(*window.document()));

    auto* container =
        Supplement<LocalDOMWindow>::From<ServiceWorkerContainer>(window);
    if (!container) {
      // There is no container for some reason, which means there's no message
      // queue to start. Just abort.
      return;
    }

    container->EnableClientMessageQueue();
  }
};

const char ServiceWorkerContainer::kSupplementName[] = "ServiceWorkerContainer";

ServiceWorkerContainer* ServiceWorkerContainer::From(LocalDOMWindow& window) {
  ServiceWorkerContainer* container =
      Supplement<LocalDOMWindow>::From<ServiceWorkerContainer>(window);
  if (!container) {
    // TODO(leonhsl): Figure out whether it's really necessary to create an
    // instance when there's no frame or frame client for |window|.
    container = MakeGarbageCollected<ServiceWorkerContainer>(window);
    Supplement<LocalDOMWindow>::ProvideTo(window, container);
    if (window.GetFrame() && window.GetFrame()->Client()) {
      std::unique_ptr<WebServiceWorkerProvider> provider =
          window.GetFrame()->Client()->CreateServiceWorkerProvider();
      if (provider) {
        provider->SetClient(container);
        container->provider_ = std::move(provider);
      }
    }
  }
  return container;
}

ServiceWorkerContainer* ServiceWorkerContainer::CreateForTesting(
    LocalDOMWindow& window,
    std::unique_ptr<WebServiceWorkerProvider> provider) {
  ServiceWorkerContainer* container =
      MakeGarbageCollected<ServiceWorkerContainer>(window);
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
  Supplement<LocalDOMWindow>::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

ScriptPromise<ServiceWorkerRegistration>
ServiceWorkerContainer::registerServiceWorker(
    ScriptState* script_state,
    const String& url,
    const RegistrationOptions* options) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<ServiceWorkerRegistration>>(
          script_state);
  auto promise = resolver->Promise();
  auto callbacks = std::make_unique<CallbackPromiseAdapter<
      ServiceWorkerRegistration, ServiceWorkerErrorForUpdate>>(resolver);

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
    callbacks->OnError(WebServiceWorkerError(
        mojom::blink::ServiceWorkerErrorType::kType,
        String("Failed to register a ServiceWorker: The URL protocol of the "
               "current origin ('" +
               document_origin->ToString() + "') is not supported.")));
    return promise;
  }

  KURL script_url = execution_context->CompleteURL(url);
  script_url.RemoveFragmentIdentifier();

  if (!SchemeRegistry::ShouldTreatURLSchemeAsAllowingServiceWorkers(
          script_url.Protocol())) {
    callbacks->OnError(WebServiceWorkerError(
        mojom::blink::ServiceWorkerErrorType::kType,
        String("Failed to register a ServiceWorker: The URL protocol of the "
               "script ('" +
               script_url.GetString() + "') is not supported.")));
    return promise;
  }

  if (!document_origin->CanRequest(script_url)) {
    scoped_refptr<const SecurityOrigin> script_origin =
        SecurityOrigin::Create(script_url);
    callbacks->OnError(
        WebServiceWorkerError(mojom::blink::ServiceWorkerErrorType::kSecurity,
                              String("Failed to register a ServiceWorker: The "
                                     "origin of the provided scriptURL ('" +
                                     script_origin->ToString() +
                                     "') does not match the current origin ('" +
                                     document_origin->ToString() + "').")));
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
    callbacks->OnError(WebServiceWorkerError(
        mojom::blink::ServiceWorkerErrorType::kType,
        String("Failed to register a ServiceWorker: The URL protocol of the "
               "scope ('" +
               scope_url.GetString() + "') is not supported.")));
    return promise;
  }

  if (!document_origin->CanRequest(scope_url)) {
    scoped_refptr<const SecurityOrigin> scope_origin =
        SecurityOrigin::Create(scope_url);
    callbacks->OnError(
        WebServiceWorkerError(mojom::blink::ServiceWorkerErrorType::kSecurity,
                              String("Failed to register a ServiceWorker: The "
                                     "origin of the provided scope ('" +
                                     scope_origin->ToString() +
                                     "') does not match the current origin ('" +
                                     document_origin->ToString() + "').")));
    return promise;
  }

  if (!provider_) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError,
        "Failed to register a ServiceWorker: "
        "The document is in an invalid "
        "state."));
    return promise;
  }
  WebString web_error_message;
  if (!provider_->ValidateScopeAndScriptURL(scope_url, script_url,
                                            &web_error_message)) {
    callbacks->OnError(WebServiceWorkerError(
        mojom::blink::ServiceWorkerErrorType::kType,
        WebString::FromUTF8("Failed to register a ServiceWorker: " +
                            web_error_message.Utf8())));
    return promise;
  }

  ContentSecurityPolicy* csp = execution_context->GetContentSecurityPolicy();
  if (csp) {
    if (!csp->AllowWorkerContextFromSource(script_url)) {
      callbacks->OnError(WebServiceWorkerError(
          mojom::blink::ServiceWorkerErrorType::kSecurity,
          String(
              "Failed to register a ServiceWorker: The provided scriptURL ('" +
              script_url.GetString() +
              "') violates the Content Security Policy.")));
      return promise;
    }
  }

  mojom::blink::ServiceWorkerUpdateViaCache update_via_cache =
      V8EnumToUpdateViaCache(options->updateViaCache().AsEnum());
  mojom::blink::ScriptType script_type =
      Script::V8WorkerTypeToScriptType(options->type().AsEnum());

  WebFetchClientSettingsObject fetch_client_settings_object(
      execution_context->Fetcher()
          ->GetProperties()
          .GetFetchClientSettingsObject());

  // Defer register() from a prerendered page until page activation.
  // https://wicg.github.io/nav-speculation/prerendering.html#patch-service-workers
  if (GetExecutionContext()->IsWindow()) {
    Document* document = To<LocalDOMWindow>(GetExecutionContext())->document();
    if (document->IsPrerendering()) {
      document->AddPostPrerenderingActivationStep(WTF::BindOnce(
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
    std::unique_ptr<CallbackPromiseAdapter<ServiceWorkerRegistration,
                                           ServiceWorkerErrorForUpdate>>
        callbacks) {
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
        "Failed to get a ServiceWorkerRegistration: The URL protocol of the "
        "current origin ('" +
            document_origin->ToString() + "') is not supported."));
    return promise;
  }

  KURL completed_url = execution_context->CompleteURL(document_url);
  completed_url.RemoveFragmentIdentifier();
  if (!document_origin->CanRequest(completed_url)) {
    scoped_refptr<const SecurityOrigin> document_url_origin =
        SecurityOrigin::Create(completed_url);
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kSecurityError,
        "Failed to get a ServiceWorkerRegistration: The "
        "origin of the provided documentURL ('" +
            document_url_origin->ToString() +
            "') does not match the current origin ('" +
            document_origin->ToString() + "')."));
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
        "Failed to get ServiceWorkerRegistration objects: The URL protocol of "
        "the current origin ('" +
            document_origin->ToString() + "') is not supported."));
    return promise;
  }

  provider_->GetRegistrations(
      std::make_unique<CallbackPromiseAdapter<ServiceWorkerRegistrationArray,
                                              ServiceWorkerError>>(resolver));

  return promise;
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
          WTF::BindOnce(&ServiceWorkerContainer::OnGetRegistrationForReady,
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
  auto* window = DynamicTo<LocalDOMWindow>(GetExecutionContext());
  if (!window)
    return;
  // ServiceWorkerContainer is only supported on documents.
  auto* document = window->document();
  DCHECK(document);

  if (!is_client_message_queue_enabled_) {
    if (!HasFiredDomContentLoaded(*document)) {
      // Wait for DOMContentLoaded. This corresponds to the specification steps
      // for "Parsing HTML documents": "The end" at
      // https://html.spec.whatwg.org/C/#the-end:
      //
      // 1. Fire an event named DOMContentLoaded at the Document object, with
      // its bubbles attribute initialized to true.
      // 2. Enable the client message queue of the ServiceWorkerContainer object
      // whose associated service worker client is the Document object's
      // relevant settings object.
      if (!dom_content_loaded_observer_) {
        dom_content_loaded_observer_ =
            MakeGarbageCollected<DomContentLoadedListener>();
        document->addEventListener(event_type_names::kDOMContentLoaded,
                                   dom_content_loaded_observer_.Get(), false);
      }
      queued_messages_.emplace_back(std::make_unique<MessageFromServiceWorker>(
          std::move(source), std::move(message)));
      // The messages will be dispatched once EnableClientMessageQueue() is
      // called.
      return;
    }

    // DOMContentLoaded was fired already, so enable the queue.
    EnableClientMessageQueue();
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
  return GetSupplementable()->GetExecutionContext();
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
      MakeGarbageCollected<ServiceWorkerRegistration>(
          GetSupplementable()->GetExecutionContext(), std::move(info));
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
  ServiceWorker* worker = ServiceWorker::Create(
      GetSupplementable()->GetExecutionContext(), std::move(info));
  service_worker_objects_.Set(version_id, worker);
  return worker;
}

ServiceWorkerContainer::ServiceWorkerContainer(LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window),
      ExecutionContextLifecycleObserver(&window) {}

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
  MessagePortArray* ports =
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
          GetExecutionContext()->GetSecurityOrigin()->ToString(),
          service_worker);
    }
  }
  if (!event) {
    auto* context = GetExecutionContext();
    if ((!msg.locked_to_sender_agent_cluster ||
         context->IsSameAgentCluster(msg.sender_agent_cluster_id)) &&
        msg.message->CanDeserializeIn(context)) {
      event = MessageEvent::Create(ports, std::move(msg.message),
                                   context->GetSecurityOrigin()->ToString(),
                                   String() /* lastEventId */, service_worker);
    } else {
      event = MessageEvent::CreateError(
          context->GetSecurityOrigin()->ToString(), service_worker);
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

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
#include <utility>

#include "base/macros.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_error_type.mojom-blink.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_provider.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/renderer/bindings/core/v8/callback_promise_adapter.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value_factory.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/events/message_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/messaging/blink_transferable_message.h"
#include "third_party/blink/renderer/core/messaging/message_port.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/service_worker/navigator_service_worker.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_container_client.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_error.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_registration.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_violation_reporting_policy.h"

namespace blink {

namespace {

mojom::ServiceWorkerUpdateViaCache ParseUpdateViaCache(const String& value) {
  if (value == "imports")
    return mojom::ServiceWorkerUpdateViaCache::kImports;
  if (value == "all")
    return mojom::ServiceWorkerUpdateViaCache::kAll;
  if (value == "none")
    return mojom::ServiceWorkerUpdateViaCache::kNone;
  // Default value.
  return mojom::ServiceWorkerUpdateViaCache::kImports;
}

mojom::ScriptType ParseScriptType(const String& type) {
  if (type == "classic")
    return mojom::ScriptType::kClassic;
  if (type == "module")
    return mojom::ScriptType::kModule;
  NOTREACHED() << "Invalid type: " << type;
  return mojom::ScriptType::kClassic;
}

class GetRegistrationCallback : public WebServiceWorkerProvider::
                                    WebServiceWorkerGetRegistrationCallbacks {
 public:
  explicit GetRegistrationCallback(ScriptPromiseResolver* resolver)
      : resolver_(resolver) {}
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
  Persistent<ScriptPromiseResolver> resolver_;
  DISALLOW_COPY_AND_ASSIGN(GetRegistrationCallback);
};

}  // namespace

class ServiceWorkerContainer::GetRegistrationForReadyCallback
    : public WebServiceWorkerProvider::
          WebServiceWorkerGetRegistrationForReadyCallbacks {
 public:
  explicit GetRegistrationForReadyCallback(ReadyProperty* ready)
      : ready_(ready) {}
  ~GetRegistrationForReadyCallback() override = default;

  void OnSuccess(WebServiceWorkerRegistrationObjectInfo info) override {
    DCHECK_EQ(ready_->GetState(), ReadyProperty::kPending);

    if (ready_->GetExecutionContext() &&
        !ready_->GetExecutionContext()->IsContextDestroyed()) {
      ready_->Resolve(
          ServiceWorkerContainerClient::From(
              To<Document>(ready_->GetExecutionContext()))
              ->GetOrCreateServiceWorkerRegistration(std::move(info)));
    }
  }

 private:
  Persistent<ReadyProperty> ready_;
  DISALLOW_COPY_AND_ASSIGN(GetRegistrationForReadyCallback);
};

ServiceWorkerContainer* ServiceWorkerContainer::Create(
    ExecutionContext* execution_context,
    NavigatorServiceWorker* navigator) {
  return new ServiceWorkerContainer(execution_context, navigator);
}

ServiceWorkerContainer::~ServiceWorkerContainer() {
  DCHECK(!provider_);
}

void ServiceWorkerContainer::ContextDestroyed(ExecutionContext*) {
  if (provider_) {
    provider_->SetClient(nullptr);
    provider_ = nullptr;
  }
  controller_ = nullptr;
  navigator_->ClearServiceWorker();
}

void ServiceWorkerContainer::Trace(blink::Visitor* visitor) {
  visitor->Trace(controller_);
  visitor->Trace(ready_);
  visitor->Trace(navigator_);
  EventTargetWithInlineData::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

ScriptPromise ServiceWorkerContainer::registerServiceWorker(
    ScriptState* script_state,
    const String& url,
    const RegistrationOptions& options) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  if (!provider_) {
    resolver->Reject(DOMException::Create(DOMExceptionCode::kInvalidStateError,
                                          "Failed to register a ServiceWorker: "
                                          "The document is in an invalid "
                                          "state."));
    return promise;
  }

  // TODO(asamidoi): Remove this check after module loading for
  // ServiceWorker is enabled by default (https://crbug.com/824647).
  if (options.type() == "module" &&
      !RuntimeEnabledFeatures::ModuleServiceWorkerEnabled()) {
    resolver->Reject(DOMException::Create(
        DOMExceptionCode::kNotSupportedError,
        "type 'module' in RegistrationOptions is not implemented yet."
        "See https://crbug.com/824647 for details."));
    return promise;
  }

  auto callbacks = std::make_unique<CallbackPromiseAdapter<
      ServiceWorkerRegistration, ServiceWorkerErrorForUpdate>>(resolver);

  ExecutionContext* execution_context = ExecutionContext::From(script_state);

  // The IDL definition is expected to restrict service worker to secure
  // contexts.
  CHECK(execution_context->IsSecureContext());

  scoped_refptr<const SecurityOrigin> document_origin =
      execution_context->GetSecurityOrigin();
  KURL page_url = KURL(NullURL(), document_origin->ToString());
  if (!SchemeRegistry::ShouldTreatURLSchemeAsAllowingServiceWorkers(
          page_url.Protocol())) {
    callbacks->OnError(WebServiceWorkerError(
        mojom::blink::ServiceWorkerErrorType::kSecurity,
        String("Failed to register a ServiceWorker: The URL protocol of the "
               "current origin ('" +
               document_origin->ToString() + "') is not supported.")));
    return promise;
  }

  KURL script_url = execution_context->CompleteURL(url);
  script_url.RemoveFragmentIdentifier();

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
  if (!SchemeRegistry::ShouldTreatURLSchemeAsAllowingServiceWorkers(
          script_url.Protocol())) {
    callbacks->OnError(WebServiceWorkerError(
        mojom::blink::ServiceWorkerErrorType::kSecurity,
        String("Failed to register a ServiceWorker: The URL protocol of the "
               "script ('" +
               script_url.GetString() + "') is not supported.")));
    return promise;
  }

  KURL pattern_url;
  if (options.scope().IsNull())
    pattern_url = KURL(script_url, "./");
  else
    pattern_url = execution_context->CompleteURL(options.scope());
  pattern_url.RemoveFragmentIdentifier();

  if (!document_origin->CanRequest(pattern_url)) {
    scoped_refptr<const SecurityOrigin> pattern_origin =
        SecurityOrigin::Create(pattern_url);
    callbacks->OnError(
        WebServiceWorkerError(mojom::blink::ServiceWorkerErrorType::kSecurity,
                              String("Failed to register a ServiceWorker: The "
                                     "origin of the provided scope ('" +
                                     pattern_origin->ToString() +
                                     "') does not match the current origin ('" +
                                     document_origin->ToString() + "').")));
    return promise;
  }
  if (!SchemeRegistry::ShouldTreatURLSchemeAsAllowingServiceWorkers(
          pattern_url.Protocol())) {
    callbacks->OnError(WebServiceWorkerError(
        mojom::blink::ServiceWorkerErrorType::kSecurity,
        String("Failed to register a ServiceWorker: The URL protocol of the "
               "scope ('" +
               pattern_url.GetString() + "') is not supported.")));
    return promise;
  }

  WebString web_error_message;
  if (!provider_->ValidateScopeAndScriptURL(pattern_url, script_url,
                                            &web_error_message)) {
    callbacks->OnError(WebServiceWorkerError(
        mojom::blink::ServiceWorkerErrorType::kType,
        WebString::FromUTF8("Failed to register a ServiceWorker: " +
                            web_error_message.Utf8())));
    return promise;
  }

  ContentSecurityPolicy* csp = execution_context->GetContentSecurityPolicy();
  if (csp) {
    if (!csp->AllowRequestWithoutIntegrity(
            mojom::RequestContextType::SERVICE_WORKER, script_url) ||
        !csp->AllowWorkerContextFromSource(
            script_url, ResourceRequest::RedirectStatus::kNoRedirect,
            SecurityViolationReportingPolicy::kReport)) {
      callbacks->OnError(WebServiceWorkerError(
          mojom::blink::ServiceWorkerErrorType::kSecurity,
          String(
              "Failed to register a ServiceWorker: The provided scriptURL ('" +
              script_url.GetString() +
              "') violates the Content Security Policy.")));
      return promise;
    }
  }

  mojom::ServiceWorkerUpdateViaCache update_via_cache =
      ParseUpdateViaCache(options.updateViaCache());
  mojom::ScriptType type = ParseScriptType(options.type());

  provider_->RegisterServiceWorker(pattern_url, script_url, type,
                                   update_via_cache, std::move(callbacks));
  return promise;
}

ScriptPromise ServiceWorkerContainer::getRegistration(
    ScriptState* script_state,
    const String& document_url) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  if (!provider_) {
    resolver->Reject(DOMException::Create(DOMExceptionCode::kInvalidStateError,
                                          "Failed to get a "
                                          "ServiceWorkerRegistration: The "
                                          "document is in an invalid state."));
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
    resolver->Reject(DOMException::Create(
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
    resolver->Reject(
        DOMException::Create(DOMExceptionCode::kSecurityError,
                             "Failed to get a ServiceWorkerRegistration: The "
                             "origin of the provided documentURL ('" +
                                 document_url_origin->ToString() +
                                 "') does not match the current origin ('" +
                                 document_origin->ToString() + "')."));
    return promise;
  }
  provider_->GetRegistration(
      completed_url, std::make_unique<GetRegistrationCallback>(resolver));

  return promise;
}

ScriptPromise ServiceWorkerContainer::getRegistrations(
    ScriptState* script_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  if (!provider_) {
    resolver->Reject(
        DOMException::Create(DOMExceptionCode::kInvalidStateError,
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
    resolver->Reject(DOMException::Create(
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

ScriptPromise ServiceWorkerContainer::ready(ScriptState* caller_state) {
  if (!GetExecutionContext())
    return ScriptPromise();

  if (!caller_state->World().IsMainWorld()) {
    // FIXME: Support .ready from isolated worlds when
    // ScriptPromiseProperty can vend Promises in isolated worlds.
    return ScriptPromise::RejectWithDOMException(
        caller_state,
        DOMException::Create(DOMExceptionCode::kNotSupportedError,
                             "'ready' is only supported in pages."));
  }

  if (!ready_) {
    ready_ = CreateReadyProperty();
    if (provider_) {
      provider_->GetRegistrationForReady(
          std::make_unique<GetRegistrationForReadyCallback>(ready_.Get()));
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
    UseCounter::Count(GetExecutionContext(),
                      WebFeature::kServiceWorkerControlledPage);
  }
  if (should_notify_controller_change)
    DispatchEvent(*Event::Create(EventTypeNames::controllerchange));
}

void ServiceWorkerContainer::DispatchMessageEvent(
    WebServiceWorkerObjectInfo info,
    TransferableMessage message) {
  if (!GetExecutionContext() || !GetExecutionContext()->ExecutingWindow())
    return;
  auto msg = ToBlinkTransferableMessage(std::move(message));
  MessagePortArray* ports =
      MessagePort::EntanglePorts(*GetExecutionContext(), std::move(msg.ports));
  ServiceWorker* source =
      ServiceWorker::From(GetExecutionContext(), std::move(info));
  MessageEvent* event;
  if (!msg.locked_agent_cluster_id ||
      GetExecutionContext()->IsSameAgentCluster(*msg.locked_agent_cluster_id)) {
    event = MessageEvent::Create(
        ports, std::move(msg.message),
        GetExecutionContext()->GetSecurityOrigin()->ToString(),
        String() /* lastEventId */, source);
  } else {
    event = MessageEvent::CreateError(
        GetExecutionContext()->GetSecurityOrigin()->ToString(), source);
  }
  DispatchEvent(*event);
}

void ServiceWorkerContainer::CountFeature(mojom::WebFeature feature) {
  if (!GetExecutionContext())
    return;
  if (Deprecation::DeprecationMessage(feature).IsEmpty())
    UseCounter::Count(GetExecutionContext(), feature);
  else
    Deprecation::CountDeprecation(GetExecutionContext(), feature);
}

const AtomicString& ServiceWorkerContainer::InterfaceName() const {
  return EventTargetNames::ServiceWorkerContainer;
}

ServiceWorkerContainer::ServiceWorkerContainer(
    ExecutionContext* execution_context,
    NavigatorServiceWorker* navigator)
    : ContextLifecycleObserver(execution_context),
      provider_(nullptr),
      navigator_(navigator) {
  if (!execution_context)
    return;

  if (ServiceWorkerContainerClient* client =
          ServiceWorkerContainerClient::From(To<Document>(execution_context))) {
    provider_ = client->Provider();
    if (provider_)
      provider_->SetClient(this);
  }
}

ServiceWorkerContainer::ReadyProperty*
ServiceWorkerContainer::CreateReadyProperty() {
  return new ReadyProperty(GetExecutionContext(), this, ReadyProperty::kReady);
}

}  // namespace blink

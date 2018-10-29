// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/web_service_worker_provider_impl.h"

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/renderer/service_worker/service_worker_provider_context.h"
#include "content/renderer/service_worker/service_worker_type_converters.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider_type.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_error.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_provider_client.h"
#include "third_party/blink/public/platform/web_url.h"

using blink::WebURL;

namespace content {

namespace {

const char kLostConnectionErrorMessage[] =
    "Lost connection to the service worker system.";

}  // anonymous namespace

WebServiceWorkerProviderImpl::WebServiceWorkerProviderImpl(
    ServiceWorkerProviderContext* context)
    : context_(context), provider_client_(nullptr), weak_factory_(this) {
  DCHECK(context_);
  switch (context_->provider_type()) {
    case blink::mojom::ServiceWorkerProviderType::kForWindow:
      DCHECK(context_->container_host());
      context_->SetWebServiceWorkerProvider(weak_factory_.GetWeakPtr());
      break;
    case blink::mojom::ServiceWorkerProviderType::kForServiceWorker:
      // Do nothing.
      break;
    case blink::mojom::ServiceWorkerProviderType::kForSharedWorker:
    case blink::mojom::ServiceWorkerProviderType::kUnknown:
      NOTREACHED() << "Unimplemented type: " << context_->provider_type();
      break;
  }
}

WebServiceWorkerProviderImpl::~WebServiceWorkerProviderImpl() = default;

void WebServiceWorkerProviderImpl::SetClient(
    blink::WebServiceWorkerProviderClient* client) {
  provider_client_ = client;
  if (!provider_client_)
    return;

  blink::mojom::ServiceWorkerObjectInfoPtr controller =
      context_->TakeController();
  if (!controller)
    return;
  DCHECK_NE(blink::mojom::kInvalidServiceWorkerVersionId,
            controller->version_id);
  SetController(std::move(controller), context_->used_features(),
                false /* notify_controllerchange */);
}

void WebServiceWorkerProviderImpl::RegisterServiceWorker(
    const WebURL& web_pattern,
    const WebURL& web_script_url,
    blink::mojom::ScriptType script_type,
    blink::mojom::ServiceWorkerUpdateViaCache update_via_cache,
    std::unique_ptr<WebServiceWorkerRegistrationCallbacks> callbacks) {
  DCHECK(callbacks);

  GURL pattern(web_pattern);
  GURL script_url(web_script_url);
  if (pattern.possibly_invalid_spec().size() > url::kMaxURLChars ||
      script_url.possibly_invalid_spec().size() > url::kMaxURLChars) {
    std::string error_message(kServiceWorkerRegisterErrorPrefix);
    error_message += "The provided scriptURL or scope is too long.";
    callbacks->OnError(blink::WebServiceWorkerError(
        blink::mojom::ServiceWorkerErrorType::kSecurity,
        blink::WebString::FromASCII(error_message)));
    return;
  }

  if (!context_->container_host()) {
    std::string error_message(kServiceWorkerRegisterErrorPrefix);
    error_message += kLostConnectionErrorMessage;
    callbacks->OnError(blink::WebServiceWorkerError(
        blink::mojom::ServiceWorkerErrorType::kAbort,
        blink::WebString::FromASCII(error_message)));
    return;
  }

  TRACE_EVENT_ASYNC_BEGIN2(
      "ServiceWorker", "WebServiceWorkerProviderImpl::RegisterServiceWorker",
      this, "Scope", pattern.spec(), "Script URL", script_url.spec());

  // TODO(asamidoi): Create this options in
  // ServiceWorkerContainer::RegisterServiceWorker() and pass it as an argument
  // in this function instead of blink::mojom::ScriptType and
  // blink::mojom::ServiceWorkerUpdateViaCache.
  auto options = blink::mojom::ServiceWorkerRegistrationOptions::New(
      pattern, script_type, update_via_cache);
  context_->container_host()->Register(
      script_url, std::move(options),
      base::BindOnce(&WebServiceWorkerProviderImpl::OnRegistered,
                     weak_factory_.GetWeakPtr(), std::move(callbacks)));
}

void WebServiceWorkerProviderImpl::GetRegistration(
    const blink::WebURL& web_document_url,
    std::unique_ptr<WebServiceWorkerGetRegistrationCallbacks> callbacks) {
  DCHECK(callbacks);
  GURL document_url(web_document_url);
  if (document_url.possibly_invalid_spec().size() > url::kMaxURLChars) {
    std::string error_message(kServiceWorkerGetRegistrationErrorPrefix);
    error_message += "The provided documentURL is too long.";
    callbacks->OnError(blink::WebServiceWorkerError(
        blink::mojom::ServiceWorkerErrorType::kSecurity,
        blink::WebString::FromASCII(error_message)));
    return;
  }

  if (!context_->container_host()) {
    std::string error_message(kServiceWorkerGetRegistrationErrorPrefix);
    error_message += kLostConnectionErrorMessage;
    callbacks->OnError(blink::WebServiceWorkerError(
        blink::mojom::ServiceWorkerErrorType::kAbort,
        blink::WebString::FromASCII(error_message)));
    return;
  }

  TRACE_EVENT_ASYNC_BEGIN1("ServiceWorker",
                           "WebServiceWorkerProviderImpl::GetRegistration",
                           this, "Document URL", document_url.spec());
  context_->container_host()->GetRegistration(
      document_url,
      base::BindOnce(&WebServiceWorkerProviderImpl::OnDidGetRegistration,
                     weak_factory_.GetWeakPtr(), std::move(callbacks)));
}

void WebServiceWorkerProviderImpl::GetRegistrations(
    std::unique_ptr<WebServiceWorkerGetRegistrationsCallbacks> callbacks) {
  DCHECK(callbacks);
  if (!context_->container_host()) {
    std::string error_message(kServiceWorkerGetRegistrationsErrorPrefix);
    error_message += kLostConnectionErrorMessage;
    callbacks->OnError(blink::WebServiceWorkerError(
        blink::mojom::ServiceWorkerErrorType::kAbort,
        blink::WebString::FromASCII(error_message)));
    return;
  }

  TRACE_EVENT_ASYNC_BEGIN0(
      "ServiceWorker", "WebServiceWorkerProviderImpl::GetRegistrations", this);
  context_->container_host()->GetRegistrations(
      base::BindOnce(&WebServiceWorkerProviderImpl::OnDidGetRegistrations,
                     weak_factory_.GetWeakPtr(), std::move(callbacks)));
}

void WebServiceWorkerProviderImpl::GetRegistrationForReady(
    std::unique_ptr<WebServiceWorkerGetRegistrationForReadyCallbacks>
        callbacks) {
  if (!context_->container_host()) {
    return;
  }

  TRACE_EVENT_ASYNC_BEGIN0(
      "ServiceWorker", "WebServiceWorkerProviderImpl::GetRegistrationForReady",
      this);
  context_->container_host()->GetRegistrationForReady(base::BindOnce(
      &WebServiceWorkerProviderImpl::OnDidGetRegistrationForReady,
      weak_factory_.GetWeakPtr(), std::move(callbacks)));
}

bool WebServiceWorkerProviderImpl::ValidateScopeAndScriptURL(
    const blink::WebURL& scope,
    const blink::WebURL& script_url,
    blink::WebString* error_message) {
  std::string error;
  bool has_error = ServiceWorkerUtils::ContainsDisallowedCharacter(
      scope, script_url, &error);
  if (has_error)
    *error_message = blink::WebString::FromUTF8(error);
  return !has_error;
}

void WebServiceWorkerProviderImpl::SetController(
    blink::mojom::ServiceWorkerObjectInfoPtr controller,
    const std::set<blink::mojom::WebFeature>& features,
    bool should_notify_controller_change) {
  if (!provider_client_)
    return;

  for (blink::mojom::WebFeature feature : features)
    provider_client_->CountFeature(feature);
  provider_client_->SetController(
      controller.To<blink::WebServiceWorkerObjectInfo>(),
      should_notify_controller_change);
}

void WebServiceWorkerProviderImpl::PostMessageToClient(
    blink::mojom::ServiceWorkerObjectInfoPtr source,
    blink::TransferableMessage message) {
  if (!provider_client_)
    return;

  provider_client_->DispatchMessageEvent(
      source.To<blink::WebServiceWorkerObjectInfo>(), std::move(message));
}

void WebServiceWorkerProviderImpl::CountFeature(
    blink::mojom::WebFeature feature) {
  if (!provider_client_)
    return;
  provider_client_->CountFeature(feature);
}

void WebServiceWorkerProviderImpl::OnRegistered(
    std::unique_ptr<WebServiceWorkerRegistrationCallbacks> callbacks,
    blink::mojom::ServiceWorkerErrorType error,
    const base::Optional<std::string>& error_msg,
    blink::mojom::ServiceWorkerRegistrationObjectInfoPtr registration) {
  TRACE_EVENT_ASYNC_END2(
      "ServiceWorker", "WebServiceWorkerProviderImpl::RegisterServiceWorker",
      this, "Error", ServiceWorkerUtils::MojoEnumToString(error), "Message",
      error_msg ? *error_msg : "Success");
  if (error != blink::mojom::ServiceWorkerErrorType::kNone) {
    DCHECK(error_msg);
    DCHECK(!registration);
    callbacks->OnError(blink::WebServiceWorkerError(
        error, blink::WebString::FromASCII(*error_msg)));
    return;
  }

  DCHECK(!error_msg);
  DCHECK(registration);
  DCHECK_NE(blink::mojom::kInvalidServiceWorkerRegistrationId,
            registration->registration_id);
  callbacks->OnSuccess(
      registration.To<blink::WebServiceWorkerRegistrationObjectInfo>());
}

void WebServiceWorkerProviderImpl::OnDidGetRegistration(
    std::unique_ptr<WebServiceWorkerGetRegistrationCallbacks> callbacks,
    blink::mojom::ServiceWorkerErrorType error,
    const base::Optional<std::string>& error_msg,
    blink::mojom::ServiceWorkerRegistrationObjectInfoPtr registration) {
  TRACE_EVENT_ASYNC_END2("ServiceWorker",
                         "WebServiceWorkerProviderImpl::GetRegistration", this,
                         "Error", ServiceWorkerUtils::MojoEnumToString(error),
                         "Message", error_msg ? *error_msg : "Success");
  if (error != blink::mojom::ServiceWorkerErrorType::kNone) {
    DCHECK(error_msg);
    DCHECK(!registration);
    callbacks->OnError(blink::WebServiceWorkerError(
        error, blink::WebString::FromASCII(*error_msg)));
    return;
  }

  DCHECK(!error_msg);
  // |registration| is nullptr if there is no registration at the scope or it's
  // uninstalling.
  DCHECK(!registration ||
         registration->registration_id !=
             blink::mojom::kInvalidServiceWorkerRegistrationId);
  callbacks->OnSuccess(
      registration.To<blink::WebServiceWorkerRegistrationObjectInfo>());
}

void WebServiceWorkerProviderImpl::OnDidGetRegistrations(
    std::unique_ptr<WebServiceWorkerGetRegistrationsCallbacks> callbacks,
    blink::mojom::ServiceWorkerErrorType error,
    const base::Optional<std::string>& error_msg,
    base::Optional<
        std::vector<blink::mojom::ServiceWorkerRegistrationObjectInfoPtr>>
        infos) {
  TRACE_EVENT_ASYNC_END2("ServiceWorker",
                         "WebServiceWorkerProviderImpl::GetRegistrations", this,
                         "Error", ServiceWorkerUtils::MojoEnumToString(error),
                         "Message", error_msg ? *error_msg : "Success");
  if (error != blink::mojom::ServiceWorkerErrorType::kNone) {
    DCHECK(error_msg);
    DCHECK(!infos);
    callbacks->OnError(blink::WebServiceWorkerError(
        error, blink::WebString::FromASCII(*error_msg)));
    return;
  }

  DCHECK(!error_msg);
  DCHECK(infos);
  blink::WebVector<blink::WebServiceWorkerRegistrationObjectInfo> registrations;
  registrations.reserve(infos->size());
  for (size_t i = 0; i < infos->size(); ++i) {
    DCHECK_NE(blink::mojom::kInvalidServiceWorkerRegistrationId,
              (*infos)[i]->registration_id);
    registrations.emplace_back(
        (*infos)[i].To<blink::WebServiceWorkerRegistrationObjectInfo>());
  }
  callbacks->OnSuccess(std::move(registrations));
}

void WebServiceWorkerProviderImpl::OnDidGetRegistrationForReady(
    std::unique_ptr<WebServiceWorkerGetRegistrationForReadyCallbacks> callbacks,
    blink::mojom::ServiceWorkerRegistrationObjectInfoPtr registration) {
  TRACE_EVENT_ASYNC_END0(
      "ServiceWorker", "WebServiceWorkerProviderImpl::GetRegistrationForReady",
      this);
  // TODO(leonhsl): Currently the only reason that we allow nullable
  // |registration| is: impl of the mojo method
  // GetRegistrationForReady() needs to respond some non-sense params even if it
  // has found that the request is a bad message and has called
  // mojo::ReportBadMessage(), this is forced by Mojo, please see
  // content::ServiceWorkerProviderHost::GetRegistrationForReady(). We'll find a
  // better solution once the discussion at
  // https://groups.google.com/a/chromium.org/forum/#!topic/chromium-mojo/NNsogKNurlA
  // settled.
  CHECK(registration);
  DCHECK_NE(blink::mojom::kInvalidServiceWorkerRegistrationId,
            registration->registration_id);
  callbacks->OnSuccess(
      registration.To<blink::WebServiceWorkerRegistrationObjectInfo>());
}

}  // namespace content

// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/web_service_worker_provider_impl.h"

#include <memory>
#include <utility>

#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "content/renderer/service_worker/service_worker_provider_context.h"
#include "content/renderer/service_worker/service_worker_type_converters.h"
#include "content/renderer/worker/fetch_client_settings_object_helpers.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/public/common/service_worker/service_worker_scope_match.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_container_type.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration_options.mojom.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_error.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_provider_client.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

using blink::WebURL;

namespace content {

namespace {

template <typename T>
static std::string MojoEnumToString(T mojo_enum) {
  std::ostringstream oss;
  oss << mojo_enum;
  return oss.str();
}

bool IsValidContext(ServiceWorkerProviderContext* context) {
  return context->container_type() ==
             blink::mojom::ServiceWorkerContainerType::kForWindow ||
         (base::FeatureList::IsEnabled(
              blink::features::kServiceWorkerInDedicatedWorker) &&
          context->container_type() ==
              blink::mojom::ServiceWorkerContainerType::kForDedicatedWorker);
}

}  // anonymous namespace

WebServiceWorkerProviderImpl::WebServiceWorkerProviderImpl(
    ServiceWorkerProviderContext* context)
    : context_(context), provider_client_(nullptr) {
  DCHECK(context_);
  DCHECK(IsValidContext(context_.get()));
  context_->SetWebServiceWorkerProvider(weak_factory_.GetWeakPtr());
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
    const blink::WebFetchClientSettingsObject& fetch_client_settings_object,
    std::unique_ptr<WebServiceWorkerRegistrationCallbacks> callbacks) {
  DCHECK(callbacks);

  GURL pattern(web_pattern);
  GURL script_url(web_script_url);
  const std::string error_prefix("Failed to register a ServiceWorker: ");
  if (pattern.possibly_invalid_spec().size() > url::kMaxURLChars ||
      script_url.possibly_invalid_spec().size() > url::kMaxURLChars) {
    callbacks->OnError(blink::WebServiceWorkerError(
        blink::mojom::ServiceWorkerErrorType::kSecurity,
        blink::WebString::FromASCII(
            error_prefix + "The provided scriptURL or scope is too long.")));
    return;
  }

  // TODO(asamidoi): Create this options in
  // ServiceWorkerContainer::RegisterServiceWorker() and pass it as an argument
  // in this function instead of blink::mojom::ScriptType and
  // blink::mojom::ServiceWorkerUpdateViaCache.
  auto options = blink::mojom::ServiceWorkerRegistrationOptions::New(
      pattern, script_type, update_via_cache);
  context_->Register(
      script_url, std::move(options),
      FetchClientSettingsObjectFromWebToMojom(fetch_client_settings_object),
      base::BindOnce(&WebServiceWorkerProviderImpl::OnRegistered,
                     weak_factory_.GetWeakPtr(), std::move(callbacks)));
}

void WebServiceWorkerProviderImpl::GetRegistration(
    const blink::WebURL& web_document_url,
    std::unique_ptr<WebServiceWorkerGetRegistrationCallbacks> callbacks) {
  DCHECK(callbacks);
  GURL document_url(web_document_url);
  const std::string error_prefix("Failed to get a ServiceWorkerRegistration: ");
  if (document_url.possibly_invalid_spec().size() > url::kMaxURLChars) {
    callbacks->OnError(blink::WebServiceWorkerError(
        blink::mojom::ServiceWorkerErrorType::kSecurity,
        blink::WebString::FromASCII(error_prefix +
                                    "The provided documentURL is too long.")));
    return;
  }

  context_->GetRegistration(
      document_url,
      base::BindOnce(&WebServiceWorkerProviderImpl::OnDidGetRegistration,
                     weak_factory_.GetWeakPtr(), std::move(callbacks)));
}

void WebServiceWorkerProviderImpl::GetRegistrations(
    std::unique_ptr<WebServiceWorkerGetRegistrationsCallbacks> callbacks) {
  context_->GetRegistrations(
      base::BindOnce(&WebServiceWorkerProviderImpl::OnDidGetRegistrations,
                     weak_factory_.GetWeakPtr(), std::move(callbacks)));
}

void WebServiceWorkerProviderImpl::GetRegistrationForReady(
    GetRegistrationForReadyCallback callback) {
  context_->GetRegistrationForReady(base::BindOnce(
      &WebServiceWorkerProviderImpl::OnDidGetRegistrationForReady,
      weak_factory_.GetWeakPtr(), std::move(callback)));
}

bool WebServiceWorkerProviderImpl::ValidateScopeAndScriptURL(
    const blink::WebURL& scope,
    const blink::WebURL& script_url,
    blink::WebString* error_message) {
  std::string error;
  bool has_error =
      blink::ServiceWorkerScopeOrScriptUrlContainsDisallowedCharacter(
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
      std::move(controller).To<blink::WebServiceWorkerObjectInfo>(),
      should_notify_controller_change);
}

void WebServiceWorkerProviderImpl::PostMessageToClient(
    blink::mojom::ServiceWorkerObjectInfoPtr source,
    blink::TransferableMessage message) {
  if (!provider_client_)
    return;

  provider_client_->ReceiveMessage(
      std::move(source).To<blink::WebServiceWorkerObjectInfo>(),
      std::move(message));
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
    const std::optional<std::string>& error_msg,
    blink::mojom::ServiceWorkerRegistrationObjectInfoPtr registration) {
  // End "WebServiceWorkerProviderImpl::RegisterServiceWorker" trace event.
  TRACE_EVENT_END("ServiceWorker", perfetto::Track::FromPointer(this), "Error",
                  MojoEnumToString(error), "Message",
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
      std::move(registration)
          .To<blink::WebServiceWorkerRegistrationObjectInfo>());
}

void WebServiceWorkerProviderImpl::OnDidGetRegistration(
    std::unique_ptr<WebServiceWorkerGetRegistrationCallbacks> callbacks,
    blink::mojom::ServiceWorkerErrorType error,
    const std::optional<std::string>& error_msg,
    blink::mojom::ServiceWorkerRegistrationObjectInfoPtr registration) {
  // End "WebServiceWorkerProviderImpl::GetRegistration" trace event.
  TRACE_EVENT_END("ServiceWorker", perfetto::Track::FromPointer(this), "Error",
                  MojoEnumToString(error), "Message",
                  error_msg ? *error_msg : "Success");
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
      std::move(registration)
          .To<blink::WebServiceWorkerRegistrationObjectInfo>());
}

void WebServiceWorkerProviderImpl::OnDidGetRegistrations(
    std::unique_ptr<WebServiceWorkerGetRegistrationsCallbacks> callbacks,
    blink::mojom::ServiceWorkerErrorType error,
    const std::optional<std::string>& error_msg,
    std::optional<
        std::vector<blink::mojom::ServiceWorkerRegistrationObjectInfoPtr>>
        infos) {
  // End "WebServiceWorkerProviderImpl::GetRegistrations" trace event.
  TRACE_EVENT_END("ServiceWorker", perfetto::Track::FromPointer(this), "Error",
                  MojoEnumToString(error), "Message",
                  error_msg ? *error_msg : "Success");
  if (error != blink::mojom::ServiceWorkerErrorType::kNone) {
    DCHECK(error_msg);
    DCHECK(!infos);
    callbacks->OnError(blink::WebServiceWorkerError(
        error, blink::WebString::FromASCII(*error_msg)));
    return;
  }

  DCHECK(!error_msg);
  DCHECK(infos);
  callbacks->OnSuccess(base::ToVector(std::move(*infos), [](auto&& info) {
    return std::move(info)
        .template To<blink::WebServiceWorkerRegistrationObjectInfo>();
  }));
}

void WebServiceWorkerProviderImpl::OnDidGetRegistrationForReady(
    GetRegistrationForReadyCallback callback,
    blink::mojom::ServiceWorkerRegistrationObjectInfoPtr registration) {
  // End "WebServiceWorkerProviderImpl::GetRegistrationForReady" trace event.
  TRACE_EVENT_END("ServiceWorker", perfetto::Track::FromPointer(this));
  // TODO(leonhsl): Currently the only reason that we allow nullable
  // |registration| is: impl of the mojo method
  // GetRegistrationForReady() needs to respond some non-sense params even if it
  // has found that the request is a bad message and has called
  // mojo::ReportBadMessage(), this is forced by Mojo, please see
  // content::ServiceWorkerContainerHost::GetRegistrationForReady(). We'll find
  // a better solution once the discussion at
  // https://groups.google.com/a/chromium.org/forum/#!topic/chromium-mojo/NNsogKNurlA
  // settled.
  CHECK(registration);
  DCHECK_NE(blink::mojom::kInvalidServiceWorkerRegistrationId,
            registration->registration_id);
  std::move(callback).Run(
      std::move(registration)
          .To<blink::WebServiceWorkerRegistrationObjectInfo>());
}

}  // namespace content

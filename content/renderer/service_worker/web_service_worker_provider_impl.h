// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_WEB_SERVICE_WORKER_PROVIDER_IMPL_H_
#define CONTENT_RENDERER_SERVICE_WORKER_WEB_SERVICE_WORKER_PROVIDER_IMPL_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/messaging/transferable_message.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_error_type.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_provider.h"

namespace blink {
class WebURL;
class WebServiceWorkerProviderClient;
}

namespace content {

class ServiceWorkerProviderContext;

// This class corresponds to one ServiceWorkerContainer interface in
// JS context (i.e. navigator.serviceWorker).
class CONTENT_EXPORT WebServiceWorkerProviderImpl
    : public blink::WebServiceWorkerProvider {
 public:
  explicit WebServiceWorkerProviderImpl(ServiceWorkerProviderContext* context);
  ~WebServiceWorkerProviderImpl() override;

  void SetClient(blink::WebServiceWorkerProviderClient* client) override;

  // blink::WebServiceWorkerProvider implementation.
  void RegisterServiceWorker(
      const blink::WebURL& web_pattern,
      const blink::WebURL& web_script_url,
      blink::mojom::ScriptType script_type,
      blink::mojom::ServiceWorkerUpdateViaCache update_via_cache,
      const blink::WebFetchClientSettingsObject& fetch_client_settings_object,
      std::unique_ptr<WebServiceWorkerRegistrationCallbacks>) override;
  void GetRegistration(
      const blink::WebURL& web_document_url,
      std::unique_ptr<WebServiceWorkerGetRegistrationCallbacks>) override;
  void GetRegistrations(
      std::unique_ptr<WebServiceWorkerGetRegistrationsCallbacks>) override;
  void GetRegistrationForReady(GetRegistrationForReadyCallback) override;
  bool ValidateScopeAndScriptURL(const blink::WebURL& pattern,
                                 const blink::WebURL& script_url,
                                 blink::WebString* error_message) override;
  // Sets the ServiceWorkerContainer#controller for this provider.
  void SetController(blink::mojom::ServiceWorkerObjectInfoPtr controller,
                     const std::set<blink::mojom::WebFeature>& features,
                     bool should_notify_controller_change);
  // Posts a message to the ServiceWorkerContainer for this provider.
  // Corresponds to Client#postMessage().
  void PostMessageToClient(blink::mojom::ServiceWorkerObjectInfoPtr source,
                           blink::TransferableMessage message);
  // For UseCounter purposes. Called when the controller service worker used a
  // feature. It is counted as if it were a feature usage from the page.
  void CountFeature(blink::mojom::WebFeature feature);

 private:
  void OnRegistered(
      std::unique_ptr<WebServiceWorkerRegistrationCallbacks> callbacks,
      blink::mojom::ServiceWorkerErrorType error,
      const base::Optional<std::string>& error_msg,
      blink::mojom::ServiceWorkerRegistrationObjectInfoPtr registration);

  void OnDidGetRegistration(
      std::unique_ptr<WebServiceWorkerGetRegistrationCallbacks> callbacks,
      blink::mojom::ServiceWorkerErrorType error,
      const base::Optional<std::string>& error_msg,
      blink::mojom::ServiceWorkerRegistrationObjectInfoPtr registration);

  void OnDidGetRegistrations(
      std::unique_ptr<WebServiceWorkerGetRegistrationsCallbacks> callbacks,
      blink::mojom::ServiceWorkerErrorType error,
      const base::Optional<std::string>& error_msg,
      base::Optional<
          std::vector<blink::mojom::ServiceWorkerRegistrationObjectInfoPtr>>
          infos);

  void OnDidGetRegistrationForReady(
      GetRegistrationForReadyCallback callback,
      blink::mojom::ServiceWorkerRegistrationObjectInfoPtr registration);

  scoped_refptr<ServiceWorkerProviderContext> context_;

  // |provider_client_| is implemented by blink::SWContainer and this pointer's
  // nullified when its execution context is destroyed. (|this| is attached to
  // the same context, but could live longer until the context is GC'ed)
  blink::WebServiceWorkerProviderClient* provider_client_;

  base::WeakPtrFactory<WebServiceWorkerProviderImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebServiceWorkerProviderImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_WEB_SERVICE_WORKER_PROVIDER_IMPL_H_

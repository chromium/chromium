// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_NETWORK_PROVIDER_FOR_FRAME_H_
#define CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_NETWORK_PROVIDER_FOR_FRAME_H_

#include <memory>

#include "content/renderer/service_worker/service_worker_provider_context.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/blink/public/mojom/service_worker/controller_service_worker.mojom-forward.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_fetch_handler_bypass_option.mojom-shared.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider.mojom-forward.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_network_provider.h"

namespace content {
class RenderFrameImpl;

// The WebServiceWorkerNetworkProvider implementation used for frames.
class ServiceWorkerNetworkProviderForFrame final
    : public blink::WebServiceWorkerNetworkProvider {
 public:
  // Creates a network provider for |frame|.
  //
  // |controller_info| contains the endpoint and object info that is needed to
  // set up the controller service worker for the client.
  // |fallback_loader_factory| is a default loader factory for fallback
  // requests, and is used when we create a subresource loader for controllees.
  // This is non-null only if the provider is created for controllees, and if
  // the loading context, e.g. a frame, provides it.
  static std::unique_ptr<ServiceWorkerNetworkProviderForFrame> Create(
      RenderFrameImpl* frame,
      blink::mojom::ServiceWorkerContainerInfoForClientPtr container_info,
      blink::mojom::ControllerServiceWorkerInfoPtr controller_info,
      scoped_refptr<network::SharedURLLoaderFactory> fallback_loader_factory);

  // Creates an invalid instance. It has a null |context()|.
  // TODO(falken): Just use null instead of this.
  static std::unique_ptr<ServiceWorkerNetworkProviderForFrame>
  CreateInvalidInstance();

  ~ServiceWorkerNetworkProviderForFrame() override;

  // Implements WebServiceWorkerNetworkProvider.
  void WillSendRequest(blink::WebURLRequest& request) override;
  scoped_refptr<network::SharedURLLoaderFactory> GetSubresourceLoaderFactory(
      const network::ResourceRequest& network_request,
      bool is_from_origin_dirty_style_sheet) override;
  blink::mojom::ControllerServiceWorkerMode GetControllerServiceWorkerMode()
      override;
  blink::mojom::ServiceWorkerFetchHandlerType GetFetchHandlerType() override;
  blink::mojom::ServiceWorkerFetchHandlerBypassOption
  GetFetchHandlerBypassOption() override;
  int64_t ControllerServiceWorkerID() override;
  void DispatchNetworkQuiet() override;

  ServiceWorkerProviderContext* context() { return context_.get(); }

 private:
  class NewDocumentObserver;

  explicit ServiceWorkerNetworkProviderForFrame(RenderFrameImpl* frame);

  void NotifyExecutionReady();

  // |context_| is null if |this| is an invalid instance, in which case there is
  // no connection to the browser process.
  scoped_refptr<ServiceWorkerProviderContext> context_;

  std::unique_ptr<NewDocumentObserver> observer_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_NETWORK_PROVIDER_FOR_FRAME_H_

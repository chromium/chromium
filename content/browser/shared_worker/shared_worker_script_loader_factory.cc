// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_worker/shared_worker_script_loader_factory.h"

#include <memory>
#include "base/feature_list.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/shared_worker/shared_worker_script_loader.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/blink/public/common/service_worker/service_worker_utils.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider_type.mojom.h"

namespace content {

SharedWorkerScriptLoaderFactory::SharedWorkerScriptLoaderFactory(
    int process_id,
    base::WeakPtr<ServiceWorkerProviderHost> service_worker_provider_host,
    base::WeakPtr<AppCacheHost> appcache_host,
    ResourceContext* resource_context,
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory)
    : process_id_(process_id),
      service_worker_provider_host_(std::move(service_worker_provider_host)),
      appcache_host_(std::move(appcache_host)),
      resource_context_(resource_context),
      loader_factory_(std::move(loader_factory)) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(blink::ServiceWorkerUtils::IsServicificationEnabled());
  DCHECK_EQ(service_worker_provider_host_->provider_type(),
            blink::mojom::ServiceWorkerProviderType::kForSharedWorker);
}

SharedWorkerScriptLoaderFactory::~SharedWorkerScriptLoaderFactory() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void SharedWorkerScriptLoaderFactory::CreateLoaderAndStart(
    network::mojom::URLLoaderRequest request,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& resource_request,
    network::mojom::URLLoaderClientPtr client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // When NetworkService is not enabled, this function is called from the
  // renderer process, so use ReportBadMessage() instead of DCHECK().
  if (!base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    // Handle only the main script (RESOURCE_TYPE_SHARED_WORKER). Import scripts
    // should go to the network loader or controller.
    if (resource_request.resource_type != RESOURCE_TYPE_SHARED_WORKER) {
      mojo::ReportBadMessage(
          "SharedWorkerScriptLoaderFactory should only get requests for shared "
          "worker scripts");
      return;
    }
    if (script_loader_) {
      mojo::ReportBadMessage(
          "SharedWorkerScriptLoaderFactory should be used only one time");
      return;
    }
  }
  DCHECK_EQ(RESOURCE_TYPE_SHARED_WORKER, resource_request.resource_type);
  DCHECK(!script_loader_);

  // Create a SharedWorkerScriptLoader to load the script.
  auto script_loader = std::make_unique<SharedWorkerScriptLoader>(
      process_id_, routing_id, request_id, options, resource_request,
      std::move(client), service_worker_provider_host_, appcache_host_,
      resource_context_, loader_factory_, traffic_annotation);
  script_loader_ = script_loader->GetWeakPtr();
  mojo::MakeStrongBinding(std::move(script_loader), std::move(request));
}

void SharedWorkerScriptLoaderFactory::Clone(
    network::mojom::URLLoaderFactoryRequest request) {
  // This method is required to support synchronous requests, which shared
  // worker script requests are not.
  NOTREACHED();
}

}  // namespace content

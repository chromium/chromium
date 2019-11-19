// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/worker_host/worker_script_loader_factory.h"

#include <memory>

#include "base/feature_list.h"
#include "content/browser/service_worker/service_worker_navigation_handle.h"
#include "content/browser/service_worker/service_worker_navigation_handle_core.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/worker_host/worker_script_fetch_initiator.h"
#include "content/browser/worker_host/worker_script_loader.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider_type.mojom.h"

namespace content {

WorkerScriptLoaderFactory::WorkerScriptLoaderFactory(
    int process_id,
    ServiceWorkerNavigationHandle* service_worker_handle,
    base::WeakPtr<AppCacheHost> appcache_host,
    const BrowserContextGetter& browser_context_getter,
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory)
    : process_id_(process_id),
      appcache_host_(std::move(appcache_host)),
      browser_context_getter_(browser_context_getter),
      loader_factory_(std::move(loader_factory)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (service_worker_handle) {
    service_worker_handle_ = service_worker_handle->AsWeakPtr();
  }
}

WorkerScriptLoaderFactory::~WorkerScriptLoaderFactory() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void WorkerScriptLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& resource_request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(resource_request.resource_type ==
             static_cast<int>(ResourceType::kWorker) ||
         resource_request.resource_type ==
             static_cast<int>(ResourceType::kSharedWorker))
      << resource_request.resource_type;
  DCHECK(!script_loader_);

  // Create a WorkerScriptLoader to load the script.
  auto script_loader = std::make_unique<WorkerScriptLoader>(
      process_id_, routing_id, request_id, options, resource_request,
      std::move(client), service_worker_handle_, appcache_host_,
      browser_context_getter_, loader_factory_, traffic_annotation);
  script_loader_ = script_loader->GetWeakPtr();
  mojo::MakeSelfOwnedReceiver(std::move(script_loader), std::move(receiver));
}

void WorkerScriptLoaderFactory::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver) {
  // This method is required to support synchronous requests, which shared
  // worker script requests are not.
  NOTREACHED();
}

}  // namespace content

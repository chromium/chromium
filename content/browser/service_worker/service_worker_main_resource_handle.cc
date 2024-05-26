// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_main_resource_handle.h"

#include <utility>

#include "base/functional/bind.h"
#include "content/browser/renderer_host/policy_container_host.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "services/network/public/cpp/cross_origin_embedder_policy.h"

namespace content {

ServiceWorkerMainResourceHandle::ServiceWorkerMainResourceHandle(
    scoped_refptr<ServiceWorkerContextWrapper> context_wrapper,
    ServiceWorkerAccessedCallback on_service_worker_accessed)
    : service_worker_accessed_callback_(std::move(on_service_worker_accessed)),
      context_wrapper_(std::move(context_wrapper)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

ServiceWorkerMainResourceHandle::~ServiceWorkerMainResourceHandle() = default;

void ServiceWorkerMainResourceHandle::set_service_worker_client(
    std::tuple<base::WeakPtr<ServiceWorkerClient>,
               blink::mojom::ServiceWorkerContainerInfoForClientPtr>
        service_worker_client_and_container_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(!service_worker_client_);

  std::tie(service_worker_client_, container_info_) =
      std::move(service_worker_client_and_container_info);

  CHECK(container_info_->host_remote.is_valid() &&
        container_info_->client_receiver.is_valid());
  CHECK(service_worker_client_);
}

}  // namespace content

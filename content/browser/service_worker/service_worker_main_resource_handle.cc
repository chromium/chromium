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
    ServiceWorkerAccessedCallback on_service_worker_accessed,
    base::WeakPtr<ServiceWorkerClient> parent_service_worker_client)
    : parent_service_worker_client_(std::move(parent_service_worker_client)),
      service_worker_accessed_callback_(std::move(on_service_worker_accessed)),
      context_wrapper_(std::move(context_wrapper)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

ServiceWorkerMainResourceHandle::~ServiceWorkerMainResourceHandle() = default;

void ServiceWorkerMainResourceHandle::set_service_worker_client(
    ScopedServiceWorkerClient scoped_service_worker_client) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(!scoped_service_worker_client_);

  scoped_service_worker_client_ = std::make_unique<ScopedServiceWorkerClient>(
      std::move(scoped_service_worker_client));

  CHECK(service_worker_client());
}

base::WeakPtr<ServiceWorkerClient>
ServiceWorkerMainResourceHandle::service_worker_client() {
  if (!scoped_service_worker_client_) {
    return nullptr;
  }
  return scoped_service_worker_client_->AsWeakPtr();
}

}  // namespace content

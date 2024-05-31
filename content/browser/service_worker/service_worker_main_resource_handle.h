// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_MAIN_RESOURCE_HANDLE_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_MAIN_RESOURCE_HANDLE_H_

#include "base/memory/weak_ptr.h"
#include "content/browser/service_worker/service_worker_accessed_callback.h"
#include "content/browser/service_worker/service_worker_container_host.h"
#include "content/common/content_export.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider.mojom.h"

namespace content {

class ServiceWorkerContextWrapper;

// The lifetime of the ServiceWorkerMainResourceHandle:
//   1) We create a ServiceWorkerMainResourceHandle without populating the
//   member service worker client.
//
//   2) If we pre-create a ServiceWorkerClient for this navigation, it
//   is passed to `set_service_worker_client()`.
//
//   3) When the navigation is ready to commit, the NavigationRequest will
//   call ScopedServiceWorkerClient::CommitResponse() to
//     - complete the initialization for the ServiceWorkerClient.
//     - take out the container info to be sent as part of navigation commit
//       IPC.
//
//   4) When the navigation finishes, the ServiceWorkerMainResourceHandle is
//   destroyed. The destructor of the ServiceWorkerMainResourceHandle destroys
//   the ScopedServiceWorkerClient which in turn leads to the destruction of an
//   unclaimed ServiceWorkerClient.
class CONTENT_EXPORT ServiceWorkerMainResourceHandle {
 public:
  ServiceWorkerMainResourceHandle(
      scoped_refptr<ServiceWorkerContextWrapper> context_wrapper,
      ServiceWorkerAccessedCallback on_service_worker_accessed);

  ServiceWorkerMainResourceHandle(const ServiceWorkerMainResourceHandle&) =
      delete;
  ServiceWorkerMainResourceHandle& operator=(
      const ServiceWorkerMainResourceHandle&) = delete;

  ~ServiceWorkerMainResourceHandle();

  ScopedServiceWorkerClient* scoped_service_worker_client() {
    return scoped_service_worker_client_.get();
  }

  void set_service_worker_client(
      ScopedServiceWorkerClient scoped_service_worker_client);

  base::WeakPtr<ServiceWorkerClient> service_worker_client();

  void set_parent_service_worker_client(
      base::WeakPtr<ServiceWorkerClient> service_worker_client) {
    DCHECK(!parent_service_worker_client_);
    parent_service_worker_client_ = std::move(service_worker_client);
  }

  base::WeakPtr<ServiceWorkerClient> parent_service_worker_client() {
    return parent_service_worker_client_;
  }

  const ServiceWorkerAccessedCallback& service_worker_accessed_callback() {
    return service_worker_accessed_callback_;
  }

  ServiceWorkerContextWrapper* context_wrapper() {
    return context_wrapper_.get();
  }

  base::WeakPtr<ServiceWorkerMainResourceHandle> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  std::unique_ptr<ScopedServiceWorkerClient> scoped_service_worker_client_;

  // Only used for workers with a blob URL.
  base::WeakPtr<ServiceWorkerClient> parent_service_worker_client_;

  ServiceWorkerAccessedCallback service_worker_accessed_callback_;

  scoped_refptr<ServiceWorkerContextWrapper> context_wrapper_;

  base::WeakPtrFactory<ServiceWorkerMainResourceHandle> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_MAIN_RESOURCE_HANDLE_H_

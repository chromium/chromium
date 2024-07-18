// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_MAIN_RESOURCE_HANDLE_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_MAIN_RESOURCE_HANDLE_H_

#include "base/memory/weak_ptr.h"
#include "content/browser/service_worker/service_worker_accessed_callback.h"
#include "content/common/content_export.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider.mojom.h"

namespace content {

class ScopedServiceWorkerClient;
class ServiceWorkerClient;
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
      ServiceWorkerAccessedCallback on_service_worker_accessed,
      base::WeakPtr<ServiceWorkerClient> parent_service_worker_client =
          nullptr);

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
  // In term of the spec, this is the request's reserved client
  // https://fetch.spec.whatwg.org/#concept-request-reserved-client
  // that works as the service worker client during the main resource fetch
  // https://w3c.github.io/ServiceWorker/#dfn-service-worker-client
  // and subsequently passed as navigation param's reserved environment
  // https://html.spec.whatwg.org/multipage/browsing-the-web.html#navigation-params-reserved-environment
  //
  // The controller of `service_worker_client` can change during navigation
  // fetch (e.g. when controller is lost or `skipWaiting()` is called) and thus
  // can be different from the `ServiceWorkerVersion` that intercepted the main
  // resource request, and the latest controller should be used as the initial
  // controller of the to-be-created global scope.
  std::unique_ptr<ScopedServiceWorkerClient> scoped_service_worker_client_;

  // Only set and used for workers with a blob URL.
  const base::WeakPtr<ServiceWorkerClient> parent_service_worker_client_;

  const ServiceWorkerAccessedCallback service_worker_accessed_callback_;

  const scoped_refptr<ServiceWorkerContextWrapper> context_wrapper_;

  base::WeakPtrFactory<ServiceWorkerMainResourceHandle> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_MAIN_RESOURCE_HANDLE_H_

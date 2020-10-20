// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_MAIN_RESOURCE_HANDLE_CORE_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_MAIN_RESOURCE_HANDLE_CORE_H_

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/service_worker/service_worker_accessed_callback.h"
#include "content/browser/service_worker/service_worker_container_host.h"
#include "content/browser/service_worker/service_worker_controllee_request_handler.h"
#include "content/common/content_export.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace network {
struct CrossOriginEmbedderPolicy;
}

namespace content {

class ServiceWorkerContextWrapper;
class ServiceWorkerMainResourceHandle;

// This class is created on the UI thread, but should only be accessed from the
// service worker core thread afterwards. It is the core thread pendant of
// ServiceWorkerMainResourceHandle. See the ServiceWorkerMainResourceHandle
// header for more details about the lifetime of both classes.
//
// TODO(crbug.com/824858): Merge this class into ServiceWorkerMainResourceHandle
// when the core thread moves to the UI thread.
class CONTENT_EXPORT ServiceWorkerMainResourceHandleCore {
 public:
  ServiceWorkerMainResourceHandleCore(
      base::WeakPtr<ServiceWorkerMainResourceHandle> ui_handle,
      ServiceWorkerContextWrapper* context_wrapper,
      ServiceWorkerAccessedCallback on_service_worker_accessed);
  ~ServiceWorkerMainResourceHandleCore();

  // Called by corresponding methods in ServiceWorkerMainResourceHandle. See
  // comments in the header of ServiceWorkerMainResourceHandle for details.
  void OnBeginNavigationCommit(
      int render_process_id,
      int render_frame_id,
      const network::CrossOriginEmbedderPolicy& cross_origin_embedder_policy,
      mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
          coep_reporter);
  void OnEndNavigationCommit();
  void OnBeginWorkerCommit(
      const network::CrossOriginEmbedderPolicy& cross_origin_embedder_policy);

  ServiceWorkerContextWrapper* context_wrapper() const {
    return context_wrapper_.get();
  }

  void set_container_host(
      base::WeakPtr<ServiceWorkerContainerHost> container_host) {
    container_host_ = std::move(container_host);
  }

  base::WeakPtr<ServiceWorkerContainerHost> container_host() {
    return container_host_;
  }

  void set_interceptor(
      std::unique_ptr<ServiceWorkerControlleeRequestHandler> interceptor) {
    interceptor_ = std::move(interceptor);
  }

  ServiceWorkerControlleeRequestHandler* interceptor() {
    return interceptor_.get();
  }

  const ServiceWorkerAccessedCallback& service_worker_accessed_callback() {
    return service_worker_accessed_callback_;
  }

  base::WeakPtr<ServiceWorkerMainResourceHandleCore> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  scoped_refptr<ServiceWorkerContextWrapper> context_wrapper_;
  base::WeakPtr<ServiceWorkerMainResourceHandle> ui_handle_;
  base::WeakPtr<ServiceWorkerContainerHost> container_host_;
  std::unique_ptr<ServiceWorkerControlleeRequestHandler> interceptor_;
  ServiceWorkerAccessedCallback service_worker_accessed_callback_;

  base::WeakPtrFactory<ServiceWorkerMainResourceHandleCore> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerMainResourceHandleCore);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_MAIN_RESOURCE_HANDLE_CORE_H_

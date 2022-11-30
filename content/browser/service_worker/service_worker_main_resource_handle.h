// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_MAIN_RESOURCE_HANDLE_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_MAIN_RESOURCE_HANDLE_H_

#include "base/memory/weak_ptr.h"
#include "content/browser/service_worker/service_worker_accessed_callback.h"
#include "content/browser/service_worker/service_worker_container_host.h"
#include "content/common/content_export.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider.mojom.h"

namespace network {
namespace mojom {
class CrossOriginEmbedderPolicyReporter;
}  // namespace mojom
}  // namespace network

namespace content {

struct GlobalRenderFrameHostId;
class ServiceWorkerContextWrapper;

// This class is used to manage the lifetime of ServiceWorkerContainerHosts
// created for main resource requests (navigations and web workers).
//
// The lifetime of the ServiceWorkerMainResourceHandle and the
// ServiceWorkerContainerHost are the following:
//   1) We create a ServiceWorkerMainResourceHandle without populating the
//   member service worker container info.
//
//   2) If we pre-create a ServiceWorkerContainerHost for this navigation, it
//   is added to ServiceWorkerContextCore and its container info is passed to
//   ServiceWorkerMainResourceHandle::OnCreatedContainerHost().
//
//   3) When the navigation is ready to commit, the NavigationRequest will
//   call ServiceWorkerMainResourceHandle::OnBeginNavigationCommit() to
//     - complete the initialization for the ServiceWorkerContainerHost.
//     - take out the container info to be sent as part of navigation commit
//       IPC.
//
//   4) When the navigation finishes, the ServiceWorkerMainResourceHandle is
//   destroyed. The destructor of the ServiceWorkerMainResourceHandle destroys
//   the container info which in turn leads to the destruction of an unclaimed
//   ServiceWorkerContainerHost.
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

  // Called after a ServiceWorkerContainerHost tied with |container_info| was
  // pre-created for the navigation.
  void OnCreatedContainerHost(
      blink::mojom::ServiceWorkerContainerInfoForClientPtr container_info);

  // Called when the navigation is ready to commit.
  // Provides |rfh_id|, and |policy_container_policies| to the pre-created
  // container host. Fills in |out_container_info| so the caller can send it to
  // the renderer process as part of the navigation commit IPC.
  // |out_container_info| can be filled as null if we failed to pre-create the
  // container host for some security reasons.
  void OnBeginNavigationCommit(
      const GlobalRenderFrameHostId& rfh_id,
      const PolicyContainerPolicies& policy_container_policies,
      mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
          coep_reporter,
      blink::mojom::ServiceWorkerContainerInfoForClientPtr* out_container_info,
      ukm::SourceId document_ukm_source_id);

  // Called after the renderer reports back that the navigation has been
  // committed.
  void OnEndNavigationCommit();

  // Similar to OnBeginNavigationCommit() for shared workers (and dedicated
  // workers when PlzDedicatedWorker is on).
  // |policy_container_policies| is passed to the pre-created container host.
  void OnBeginWorkerCommit(
      const PolicyContainerPolicies& policy_container_policies,
      ukm::SourceId worker_ukm_source_id);

  blink::mojom::ServiceWorkerContainerInfoForClientPtr TakeContainerInfo() {
    return std::move(container_info_);
  }

  bool has_container_info() const { return !!container_info_; }

  void set_container_host(
      base::WeakPtr<ServiceWorkerContainerHost> container_host) {
    container_host_ = std::move(container_host);
  }

  base::WeakPtr<ServiceWorkerContainerHost> container_host() {
    return container_host_;
  }

  void set_parent_container_host(
      base::WeakPtr<ServiceWorkerContainerHost> container_host) {
    DCHECK(!parent_container_host_);
    parent_container_host_ = std::move(container_host);
  }

  base::WeakPtr<ServiceWorkerContainerHost> parent_container_host() {
    return parent_container_host_;
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
  blink::mojom::ServiceWorkerContainerInfoForClientPtr container_info_;

  base::WeakPtr<ServiceWorkerContainerHost> container_host_;

  // Only used for workers with a blob URL.
  base::WeakPtr<ServiceWorkerContainerHost> parent_container_host_;

  ServiceWorkerAccessedCallback service_worker_accessed_callback_;

  scoped_refptr<ServiceWorkerContextWrapper> context_wrapper_;

  base::WeakPtrFactory<ServiceWorkerMainResourceHandle> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_MAIN_RESOURCE_HANDLE_H_

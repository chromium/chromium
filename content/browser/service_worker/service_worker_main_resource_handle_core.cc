// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_main_resource_handle_core.h"

#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_main_resource_handle.h"
#include "content/common/service_worker/service_worker_utils.h"

namespace content {

ServiceWorkerMainResourceHandleCore::ServiceWorkerMainResourceHandleCore(
    base::WeakPtr<ServiceWorkerMainResourceHandle> ui_handle,
    ServiceWorkerContextWrapper* context_wrapper,
    ServiceWorkerAccessedCallback service_worker_accessed_callback)
    : context_wrapper_(context_wrapper),
      ui_handle_(ui_handle),
      service_worker_accessed_callback_(
          std::move(service_worker_accessed_callback)) {
  // The ServiceWorkerMainResourceHandleCore is created on the UI thread but
  // should only be accessed from the core thread afterwards.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

ServiceWorkerMainResourceHandleCore::~ServiceWorkerMainResourceHandleCore() {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
}

void ServiceWorkerMainResourceHandleCore::OnBeginNavigationCommit(
    int render_process_id,
    int render_frame_id,
    const network::CrossOriginEmbedderPolicy& cross_origin_embedder_policy,
    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
        coep_reporter) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  if (container_host_) {
    container_host_->OnBeginNavigationCommit(render_process_id, render_frame_id,
                                             cross_origin_embedder_policy,
                                             std::move(coep_reporter));
  }
}
void ServiceWorkerMainResourceHandleCore::OnEndNavigationCommit() {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  if (container_host_)
    container_host_->OnEndNavigationCommit();
}

void ServiceWorkerMainResourceHandleCore::OnBeginWorkerCommit(
    const network::CrossOriginEmbedderPolicy& cross_origin_embedder_policy) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  if (container_host_) {
    container_host_->CompleteWebWorkerPreparation(cross_origin_embedder_policy);
  }
}

}  // namespace content

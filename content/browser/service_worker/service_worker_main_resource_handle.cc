// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_main_resource_handle.h"

#include <utility>

#include "base/bind.h"
#include "base/task/post_task.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_main_resource_handle_core.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/cpp/cross_origin_embedder_policy.h"

namespace content {

ServiceWorkerMainResourceHandle::ServiceWorkerMainResourceHandle(
    ServiceWorkerContextWrapper* context_wrapper,
    ServiceWorkerAccessedCallback on_service_worker_accessed)
    : context_wrapper_(context_wrapper) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  core_ = new ServiceWorkerMainResourceHandleCore(
      weak_factory_.GetWeakPtr(), context_wrapper,
      std::move(on_service_worker_accessed));
}

ServiceWorkerMainResourceHandle::~ServiceWorkerMainResourceHandle() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Delete the ServiceWorkerMainResourceHandleCore on the core thread.
  BrowserThread::DeleteSoon(ServiceWorkerContext::GetCoreThreadId(), FROM_HERE,
                            core_);
}

void ServiceWorkerMainResourceHandle::OnCreatedContainerHost(
    blink::mojom::ServiceWorkerContainerInfoForClientPtr container_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(container_info->host_remote.is_valid() &&
         container_info->client_receiver.is_valid());

  container_info_ = std::move(container_info);
}

void ServiceWorkerMainResourceHandle::OnBeginNavigationCommit(
    int render_process_id,
    int render_frame_id,
    const network::CrossOriginEmbedderPolicy& cross_origin_embedder_policy,
    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
        coep_reporter,
    blink::mojom::ServiceWorkerContainerInfoForClientPtr* out_container_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // We may have failed to pre-create the container host.
  if (!container_info_)
    return;
  ServiceWorkerContextWrapper::RunOrPostTaskOnCoreThread(
      FROM_HERE,
      base::BindOnce(
          &ServiceWorkerMainResourceHandleCore::OnBeginNavigationCommit,
          base::Unretained(core_), render_process_id, render_frame_id,
          cross_origin_embedder_policy, std::move(coep_reporter)));
  *out_container_info = std::move(container_info_);
}

void ServiceWorkerMainResourceHandle::OnEndNavigationCommit() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ServiceWorkerContextWrapper::RunOrPostTaskOnCoreThread(
      FROM_HERE,
      base::BindOnce(
          &ServiceWorkerMainResourceHandleCore::OnEndNavigationCommit,
          base::Unretained(core_)));
}

void ServiceWorkerMainResourceHandle::OnBeginWorkerCommit(
    const network::CrossOriginEmbedderPolicy& cross_origin_embedder_policy) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ServiceWorkerContextWrapper::RunOrPostTaskOnCoreThread(
      FROM_HERE,
      base::BindOnce(&ServiceWorkerMainResourceHandleCore::OnBeginWorkerCommit,
                     base::Unretained(core_), cross_origin_embedder_policy));
}

}  // namespace content

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_navigation_handle_core.h"

#include "base/bind.h"
#include "base/task/post_task.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_navigation_handle.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace content {

ServiceWorkerNavigationHandleCore::ServiceWorkerNavigationHandleCore(
    base::WeakPtr<ServiceWorkerNavigationHandle> ui_handle,
    ServiceWorkerContextWrapper* context_wrapper)
    : context_wrapper_(context_wrapper), ui_handle_(ui_handle) {
  // The ServiceWorkerNavigationHandleCore is created on the UI thread but
  // should only be accessed from the core thread afterwards.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

ServiceWorkerNavigationHandleCore::~ServiceWorkerNavigationHandleCore() {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
}

void ServiceWorkerNavigationHandleCore::OnCreatedProviderHost(
    base::WeakPtr<ServiceWorkerProviderHost> provider_host,
    blink::mojom::ServiceWorkerProviderInfoForClientPtr provider_info) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK(provider_host);
  provider_host_ = std::move(provider_host);

  DCHECK(provider_info->host_remote.is_valid() &&
         provider_info->client_receiver.is_valid());
  RunOrPostTaskOnThread(
      FROM_HERE, BrowserThread::UI,
      base::BindOnce(&ServiceWorkerNavigationHandle::OnCreatedProviderHost,
                     ui_handle_, std::move(provider_info)));
}

void ServiceWorkerNavigationHandleCore::OnBeginNavigationCommit(
    int render_process_id,
    int render_frame_id,
    network::mojom::CrossOriginEmbedderPolicy cross_origin_embedder_policy) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  if (provider_host_) {
    provider_host_->OnBeginNavigationCommit(render_process_id, render_frame_id,
                                            cross_origin_embedder_policy);
  }
}

void ServiceWorkerNavigationHandleCore::OnBeginWorkerCommit(
    network::mojom::CrossOriginEmbedderPolicy cross_origin_embedder_policy) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  if (provider_host_)
    provider_host_->CompleteWebWorkerPreparation(cross_origin_embedder_policy);
}

}  // namespace content

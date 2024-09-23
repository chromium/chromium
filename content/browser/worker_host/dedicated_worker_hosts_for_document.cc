// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/worker_host/dedicated_worker_hosts_for_document.h"

#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/service_worker/service_worker_client.h"
#include "content/browser/worker_host/dedicated_worker_host.h"

namespace content {

DedicatedWorkerHostsForDocument::DedicatedWorkerHostsForDocument(
    RenderFrameHost* rfh)
    : DocumentUserData<DedicatedWorkerHostsForDocument>(rfh),
      dedicated_workers_([](const base::SafeRef<DedicatedWorkerHost>& lhs,
                            const base::SafeRef<DedicatedWorkerHost>& rhs) {
        return &*lhs < &*rhs;
      }) {}

DedicatedWorkerHostsForDocument::~DedicatedWorkerHostsForDocument() = default;

void DedicatedWorkerHostsForDocument::Add(
    base::SafeRef<DedicatedWorkerHost> dedicated_worker_host) {
  dedicated_workers_.insert(dedicated_worker_host);
}

void DedicatedWorkerHostsForDocument::Remove(
    base::SafeRef<DedicatedWorkerHost> dedicated_worker_host) {
  dedicated_workers_.erase(dedicated_worker_host);
}

blink::scheduler::WebSchedulerTrackedFeatures
DedicatedWorkerHostsForDocument::GetBackForwardCacheDisablingFeatures() const {
  RenderFrameHostImpl::BackForwardCacheDisablingFeatures features;
  for (auto& feature_detail : GetBackForwardCacheBlockingDetails()) {
    features.Put(static_cast<blink::scheduler::WebSchedulerTrackedFeature>(
        feature_detail->feature));
  }
  return features;
}

DedicatedWorkerHost::BackForwardCacheBlockingDetails
DedicatedWorkerHostsForDocument::GetBackForwardCacheBlockingDetails() const {
  DedicatedWorkerHost::BackForwardCacheBlockingDetails combined_details;
  for (auto worker : dedicated_workers_) {
    auto& details_for_worker = worker->GetBackForwardCacheBlockingDetails();
    for (auto& details : details_for_worker) {
      combined_details.push_back(details.Clone());
    }
  }
  return combined_details;
}

void DedicatedWorkerHostsForDocument::OnEnterBackForwardCache() {
  DCHECK(BackForwardCache::IsBackForwardCacheFeatureEnabled());
  DCHECK_EQ(render_frame_host().GetLifecycleState(),
            RenderFrameHost::LifecycleState::kInBackForwardCache);

  for (auto worker : dedicated_workers_) {
    if (base::WeakPtr<ServiceWorkerClient> service_worker_client =
            worker->GetServiceWorkerClient()) {
      service_worker_client->OnEnterBackForwardCache();
    }
  }
}

void DedicatedWorkerHostsForDocument::OnRestoreFromBackForwardCache() {
  DCHECK(BackForwardCache::IsBackForwardCacheFeatureEnabled());
  DCHECK_EQ(render_frame_host().GetLifecycleState(),
            RenderFrameHost::LifecycleState::kInBackForwardCache);

  for (auto worker : dedicated_workers_) {
    if (base::WeakPtr<ServiceWorkerClient> service_worker_client =
            worker->GetServiceWorkerClient()) {
      service_worker_client->OnRestoreFromBackForwardCache();
    }
  }
}

void DedicatedWorkerHostsForDocument::UpdateSubresourceLoaderFactories() {
  for (auto worker : dedicated_workers_) {
    worker->UpdateSubresourceLoaderFactories();
  }
}

DOCUMENT_USER_DATA_KEY_IMPL(DedicatedWorkerHostsForDocument);

}  // namespace content

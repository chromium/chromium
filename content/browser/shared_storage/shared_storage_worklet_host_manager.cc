// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_storage/shared_storage_worklet_host_manager.h"

#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/shared_storage/shared_storage_document_service_impl.h"
#include "content/browser/shared_storage/shared_storage_worklet_host.h"

namespace content {

SharedStorageWorkletHostManager::SharedStorageWorkletHostManager() = default;
SharedStorageWorkletHostManager::~SharedStorageWorkletHostManager() = default;

void SharedStorageWorkletHostManager::OnDocumentServiceDestroyed(
    SharedStorageDocumentServiceImpl* document_service) {
  // Note: `attached_shared_storage_worklet_hosts_` will be populated when
  // there's an actual worklet operation request, but the
  // `SharedStorageDocumentServiceImpl` will call this method on destruction
  // irrespectively, so it may not exist in map.
  auto it = attached_shared_storage_worklet_hosts_.find(document_service);
  if (it == attached_shared_storage_worklet_hosts_.end())
    return;

  SharedStorageWorkletHost* worklet_host = it->second.get();
  if (!worklet_host->HasPendingOperations()) {
    attached_shared_storage_worklet_hosts_.erase(it);
    return;
  }

  DCHECK(!keep_alive_shared_storage_worklet_hosts_.count(worklet_host));
  keep_alive_shared_storage_worklet_hosts_.insert(
      {worklet_host, std::move(it->second)});

  attached_shared_storage_worklet_hosts_.erase(it);

  worklet_host->EnterKeepAliveOnDocumentDestroyed(base::BindOnce(
      &SharedStorageWorkletHostManager::OnWorkletKeepAliveFinished,
      base::Unretained(this)));
}

void SharedStorageWorkletHostManager::ExpireWorkletHostForDocumentService(
    SharedStorageDocumentServiceImpl* document_service) {
  attached_shared_storage_worklet_hosts_.erase(document_service);
}

void SharedStorageWorkletHostManager::CreateWorkletHost(
    SharedStorageDocumentServiceImpl* document_service,
    const url::Origin& frame_origin,
    const GURL& script_source_url,
    const std::vector<blink::mojom::OriginTrialFeature>& origin_trial_features,
    mojo::PendingAssociatedReceiver<blink::mojom::SharedStorageWorkletHost>
        worklet_host,
    blink::mojom::SharedStorageDocumentService::CreateWorkletCallback
        callback) {
  auto [it, inserted] = attached_shared_storage_worklet_hosts_.insert(
      {document_service,
       CreateWorkletHostHelper(*document_service, frame_origin,
                               script_source_url, origin_trial_features,
                               std::move(worklet_host), std::move(callback))});
  CHECK(inserted);
}

void SharedStorageWorkletHostManager::AddSharedStorageObserver(
    SharedStorageObserverInterface* observer) {
  observers_.AddObserver(observer);
}

void SharedStorageWorkletHostManager::RemoveSharedStorageObserver(
    SharedStorageObserverInterface* observer) {
  observers_.RemoveObserver(observer);
}

void SharedStorageWorkletHostManager::NotifySharedStorageAccessed(
    SharedStorageObserverInterface::AccessType type,
    const std::string& main_frame_id,
    const std::string& owner_origin,
    const SharedStorageEventParams& params) {
  // Don't bother getting the time if there are no observers.
  if (observers_.empty())
    return;
  base::Time now = base::Time::Now();
  for (SharedStorageObserverInterface& observer : observers_) {
    observer.OnSharedStorageAccessed(now, type, main_frame_id, owner_origin,
                                     params);
  }
}

std::unique_ptr<SharedStorageWorkletHost>
SharedStorageWorkletHostManager::CreateWorkletHostHelper(
    SharedStorageDocumentServiceImpl& document_service,
    const url::Origin& frame_origin,
    const GURL& script_source_url,
    const std::vector<blink::mojom::OriginTrialFeature>& origin_trial_features,
    mojo::PendingAssociatedReceiver<blink::mojom::SharedStorageWorkletHost>
        worklet_host,
    blink::mojom::SharedStorageDocumentService::CreateWorkletCallback
        callback) {
  return std::make_unique<SharedStorageWorkletHost>(
      document_service, frame_origin, script_source_url, origin_trial_features,
      std::move(worklet_host), std::move(callback));
}

void SharedStorageWorkletHostManager::OnWorkletKeepAliveFinished(
    SharedStorageWorkletHost* worklet_host) {
  DCHECK(keep_alive_shared_storage_worklet_hosts_.count(worklet_host));
  keep_alive_shared_storage_worklet_hosts_.erase(worklet_host);
}

void SharedStorageWorkletHostManager::NotifyUrnUuidGenerated(
    const GURL& urn_uuid) {
  for (SharedStorageObserverInterface& observer : observers_) {
    observer.OnUrnUuidGenerated(urn_uuid);
  }
}

void SharedStorageWorkletHostManager::NotifyConfigPopulated(
    const absl::optional<FencedFrameConfig>& config) {
  for (SharedStorageObserverInterface& observer : observers_) {
    observer.OnConfigPopulated(config);
  }
}

}  // namespace content

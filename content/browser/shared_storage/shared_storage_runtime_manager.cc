// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_storage/shared_storage_runtime_manager.h"

#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/shared_storage/shared_storage_document_service_impl.h"
#include "content/browser/shared_storage/shared_storage_worklet_host.h"

namespace content {

SharedStorageRuntimeManager::SharedStorageRuntimeManager() = default;
SharedStorageRuntimeManager::~SharedStorageRuntimeManager() = default;

void SharedStorageRuntimeManager::OnDocumentServiceDestroyed(
    SharedStorageDocumentServiceImpl* document_service) {
  // Note: `attached_shared_storage_worklet_hosts_` will be populated when
  // there's an actual worklet operation request, but the
  // `SharedStorageDocumentServiceImpl` will call this method on destruction
  // irrespectively, so it may not exist in map.
  auto worklet_hosts_it =
      attached_shared_storage_worklet_hosts_.find(document_service);
  if (worklet_hosts_it == attached_shared_storage_worklet_hosts_.end()) {
    return;
  }

  WorkletHosts& worklet_hosts = worklet_hosts_it->second;
  for (auto& [raw_worklet_host, worklet_host] : worklet_hosts) {
    if (raw_worklet_host->HasPendingOperations()) {
      auto [it, inserted] = keep_alive_shared_storage_worklet_hosts_.emplace(
          raw_worklet_host, std::move(worklet_host));
      CHECK(inserted);

      raw_worklet_host->EnterKeepAliveOnDocumentDestroyed(base::BindOnce(
          &SharedStorageRuntimeManager::OnWorkletKeepAliveFinished,
          base::Unretained(this)));
    }
  }

  attached_shared_storage_worklet_hosts_.erase(worklet_hosts_it);
}

void SharedStorageRuntimeManager::ExpireWorkletHostForDocumentService(
    SharedStorageDocumentServiceImpl* document_service,
    SharedStorageWorkletHost* worklet_host) {
  auto worklet_hosts_it =
      attached_shared_storage_worklet_hosts_.find(document_service);
  CHECK(worklet_hosts_it != attached_shared_storage_worklet_hosts_.end());

  WorkletHosts& worklet_hosts = worklet_hosts_it->second;

  auto worklet_host_it = worklet_hosts.find(worklet_host);
  CHECK(worklet_host_it != worklet_hosts.end());

  worklet_hosts.erase(worklet_host);

  if (worklet_hosts.empty()) {
    attached_shared_storage_worklet_hosts_.erase(worklet_hosts_it);
  }
}

void SharedStorageRuntimeManager::CreateWorkletHost(
    SharedStorageDocumentServiceImpl* document_service,
    const url::Origin& frame_origin,
    const url::Origin& data_origin,
    const GURL& script_source_url,
    network::mojom::CredentialsMode credentials_mode,
    const std::vector<blink::mojom::OriginTrialFeature>& origin_trial_features,
    mojo::PendingAssociatedReceiver<blink::mojom::SharedStorageWorkletHost>
        worklet_host_receiver,
    blink::mojom::SharedStorageDocumentService::CreateWorkletCallback
        callback) {
  auto worklet_hosts_it =
      attached_shared_storage_worklet_hosts_.find(document_service);

  // A document can only create multiple worklets with `kSharedStorageAPIM125`
  // enabled.
  if (!base::FeatureList::IsEnabled(blink::features::kSharedStorageAPIM125)) {
    CHECK(worklet_hosts_it == attached_shared_storage_worklet_hosts_.end());
  }

  WorkletHosts& worklet_hosts =
      (worklet_hosts_it != attached_shared_storage_worklet_hosts_.end())
          ? worklet_hosts_it->second
          : attached_shared_storage_worklet_hosts_[document_service];

  std::unique_ptr<SharedStorageWorkletHost> worklet_host =
      CreateWorkletHostHelper(
          *document_service, frame_origin, data_origin, script_source_url,
          credentials_mode, origin_trial_features,
          std::move(worklet_host_receiver), std::move(callback));

  SharedStorageWorkletHost* raw_worklet_host = worklet_host.get();

  worklet_hosts.emplace(raw_worklet_host, std::move(worklet_host));
}

void SharedStorageRuntimeManager::AddSharedStorageObserver(
    SharedStorageObserverInterface* observer) {
  observers_.AddObserver(observer);
}

void SharedStorageRuntimeManager::RemoveSharedStorageObserver(
    SharedStorageObserverInterface* observer) {
  observers_.RemoveObserver(observer);
}

void SharedStorageRuntimeManager::NotifySharedStorageAccessed(
    SharedStorageObserverInterface::AccessType type,
    FrameTreeNodeId main_frame_id,
    const std::string& owner_origin,
    const SharedStorageEventParams& params) {
  // Don't bother getting the time if there are no observers.
  if (observers_.empty()) {
    return;
  }
  base::Time now = base::Time::Now();
  for (SharedStorageObserverInterface& observer : observers_) {
    observer.OnSharedStorageAccessed(now, type, main_frame_id, owner_origin,
                                     params);
  }
}

std::unique_ptr<SharedStorageWorkletHost>
SharedStorageRuntimeManager::CreateWorkletHostHelper(
    SharedStorageDocumentServiceImpl& document_service,
    const url::Origin& frame_origin,
    const url::Origin& data_origin,
    const GURL& script_source_url,
    network::mojom::CredentialsMode credentials_mode,
    const std::vector<blink::mojom::OriginTrialFeature>& origin_trial_features,
    mojo::PendingAssociatedReceiver<blink::mojom::SharedStorageWorkletHost>
        worklet_host,
    blink::mojom::SharedStorageDocumentService::CreateWorkletCallback
        callback) {
  return std::make_unique<SharedStorageWorkletHost>(
      document_service, frame_origin, data_origin, script_source_url,
      credentials_mode, origin_trial_features, std::move(worklet_host),
      std::move(callback));
}

void SharedStorageRuntimeManager::OnWorkletKeepAliveFinished(
    SharedStorageWorkletHost* worklet_host) {
  DCHECK(keep_alive_shared_storage_worklet_hosts_.count(worklet_host));
  keep_alive_shared_storage_worklet_hosts_.erase(worklet_host);
}

void SharedStorageRuntimeManager::NotifyUrnUuidGenerated(const GURL& urn_uuid) {
  for (SharedStorageObserverInterface& observer : observers_) {
    observer.OnUrnUuidGenerated(urn_uuid);
  }
}

void SharedStorageRuntimeManager::NotifyConfigPopulated(
    const std::optional<FencedFrameConfig>& config) {
  for (SharedStorageObserverInterface& observer : observers_) {
    observer.OnConfigPopulated(config);
  }
}

void SharedStorageRuntimeManager::BindLockManager(
    const url::Origin& shared_storage_origin,
    mojo::PendingReceiver<blink::mojom::LockManager> receiver) {
  lock_manager_.BindReceiver(OriginLockGroupId(shared_storage_origin),
                             std::move(receiver));
}

}  // namespace content

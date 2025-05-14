// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_storage/shared_storage_runtime_manager.h"

#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/shared_storage/shared_storage_document_service_impl.h"
#include "content/browser/shared_storage/shared_storage_worklet_host.h"
#include "content/browser/storage_partition_impl.h"

namespace content {

namespace {

bool ShouldSendObserverReportForMainFrameId(
    const SharedStorageRuntimeManager::SharedStorageObserverInterface& observer,
    GlobalRenderFrameHostId main_frame_id) {
  // We should send a report if and only if (1) the observer is subscribed to
  // receiving all reports, or (2) the observer has a valid associated render
  // frame host ID (i.e. the observer is attached to a render frame host), and
  // that global render frame host ID matches the main frame ID passed as a
  // parameter of the report (and hence the observer is attached to the relevant
  // main render frame host).
  return observer.ShouldReceiveAllSharedStorageReports() ||
         (observer.AssociatedFrameHostId() &&
          observer.AssociatedFrameHostId() == main_frame_id);
}

}  // namespace

using AccessScope = blink::SharedStorageAccessScope;

SharedStorageRuntimeManager::SharedStorageRuntimeManager(
    StoragePartitionImpl& storage_partition)
    : lock_manager_(storage_partition) {}

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
    blink::mojom::SharedStorageDataOriginType data_origin_type,
    const GURL& script_source_url,
    network::mojom::CredentialsMode credentials_mode,
    blink::mojom::SharedStorageWorkletCreationMethod creation_method,
    const std::vector<blink::mojom::OriginTrialFeature>& origin_trial_features,
    mojo::PendingAssociatedReceiver<blink::mojom::SharedStorageWorkletHost>
        worklet_host_receiver,
    blink::mojom::SharedStorageDocumentService::CreateWorkletCallback
        callback) {
  auto worklet_hosts_it =
      attached_shared_storage_worklet_hosts_.find(document_service);

  WorkletHosts& worklet_hosts =
      (worklet_hosts_it != attached_shared_storage_worklet_hosts_.end())
          ? worklet_hosts_it->second
          : attached_shared_storage_worklet_hosts_[document_service];

  int worklet_ordinal_id = next_worklet_ordinal_id_++;

  std::unique_ptr<SharedStorageWorkletHost> worklet_host =
      CreateWorkletHostHelper(
          *document_service, frame_origin, data_origin, data_origin_type,
          script_source_url, credentials_mode, creation_method,
          worklet_ordinal_id, origin_trial_features,
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
    AccessScope scope,
    SharedStorageObserverInterface::AccessMethod method,
    GlobalRenderFrameHostId main_frame_id,
    const std::string& owner_origin,
    const SharedStorageEventParams& params) {
  // Don't bother getting the time if there are no observers.
  if (observers_.empty()) {
    return;
  }
  base::Time now = base::Time::Now();
  for (SharedStorageObserverInterface& observer : observers_) {
    if (!ShouldSendObserverReportForMainFrameId(observer, main_frame_id)) {
      continue;
    }
    observer.OnSharedStorageAccessed(now, scope, method, main_frame_id,
                                     owner_origin, params);
  }
}

void SharedStorageRuntimeManager::NotifyWorkletOperationExecutionFinished(
    base::TimeDelta execution_time,
    SharedStorageObserverInterface::AccessMethod method,
    int operation_id,
    int worklet_ordinal_id,
    const base::UnguessableToken& worklet_devtools_token,
    GlobalRenderFrameHostId main_frame_id,
    const std::string& owner_origin) {
  // Don't bother getting the time if there are no observers.
  if (observers_.empty()) {
    return;
  }
  base::Time now = base::Time::Now();
  for (SharedStorageObserverInterface& observer : observers_) {
    if (!ShouldSendObserverReportForMainFrameId(observer, main_frame_id)) {
      continue;
    }
    // TODO(crbug.com/401011862): Consider sending start time as well as
    // "finish" time/report time as part of the DevTools notification. Note,
    // however, that there may be a discrepancy between `execution_time` and
    // `finished_time - start-time`.
    observer.OnSharedStorageWorkletOperationExecutionFinished(
        now, execution_time, method, operation_id, worklet_ordinal_id,
        worklet_devtools_token, main_frame_id, owner_origin);
  }
}

std::unique_ptr<SharedStorageWorkletHost>
SharedStorageRuntimeManager::CreateWorkletHostHelper(
    SharedStorageDocumentServiceImpl& document_service,
    const url::Origin& frame_origin,
    const url::Origin& data_origin,
    blink::mojom::SharedStorageDataOriginType data_origin_type,
    const GURL& script_source_url,
    network::mojom::CredentialsMode credentials_mode,
    blink::mojom::SharedStorageWorkletCreationMethod creation_method,
    int worklet_ordinal_id,
    const std::vector<blink::mojom::OriginTrialFeature>& origin_trial_features,
    mojo::PendingAssociatedReceiver<blink::mojom::SharedStorageWorkletHost>
        worklet_host,
    blink::mojom::SharedStorageDocumentService::CreateWorkletCallback
        callback) {
  return std::make_unique<SharedStorageWorkletHost>(
      document_service, frame_origin, data_origin, data_origin_type,
      script_source_url, credentials_mode, creation_method, worklet_ordinal_id,
      origin_trial_features, std::move(worklet_host), std::move(callback));
}

void SharedStorageRuntimeManager::OnWorkletKeepAliveFinished(
    SharedStorageWorkletHost* worklet_host) {
  DCHECK(keep_alive_shared_storage_worklet_hosts_.count(worklet_host));
  keep_alive_shared_storage_worklet_hosts_.erase(worklet_host);
}

void SharedStorageRuntimeManager::NotifyUrnUuidGenerated(const GURL& urn_uuid) {
  for (SharedStorageObserverInterface& observer : observers_) {
    observer.OnSharedStorageSelectUrlUrnUuidGenerated(urn_uuid);
  }
}

void SharedStorageRuntimeManager::NotifyConfigPopulated(
    const std::optional<FencedFrameConfig>& config) {
  for (SharedStorageObserverInterface& observer : observers_) {
    observer.OnSharedStorageSelectUrlConfigPopulated(config);
  }
}

}  // namespace content

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/storage_access/storage_access_handle.h"

#include "base/types/pass_key.h"
#include "content/browser/broadcast_channel/broadcast_channel_provider.h"
#include "content/browser/broadcast_channel/broadcast_channel_service.h"
#include "content/browser/file_system_access/file_system_access_manager_impl.h"
#include "content/browser/network/cross_origin_embedder_policy_reporter.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/worker_host/shared_worker_connector_impl.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"

namespace content {

using PassKey = base::PassKey<StorageAccessHandle>;

namespace {

void EstimateImplAfterGetBucketUsageAndQuota(
    StorageAccessHandle::EstimateCallback callback,
    blink::mojom::QuotaStatusCode code,
    int64_t usage,
    int64_t quota) {
  if (code != blink::mojom::QuotaStatusCode::kOk) {
    std::move(callback).Run(/*usage=*/0, /*quota=*/0, /*success=*/false);
    return;
  }
  std::move(callback).Run(usage, quota, /*success=*/true);
}

}  // namespace

// static
void StorageAccessHandle::Create(
    RenderFrameHost* host,
    mojo::PendingReceiver<blink::mojom::StorageAccessHandle> receiver) {
  CHECK(host);
  // We need to check if either permission was granted *or* if full cookie
  // access was granted as pre-3PCD the permission isn't possible to set and
  // post-3PCD full cookie access remains sufficient, but is not necessary
  // as permission access is possible without enabling full cookie access.
  //
  // For more see:
  // third_party/blink/renderer/modules/storage_access/README.md
  //
  // NOTE: This handles the general permissions check for the entire interface.
  // Specific binding sights (e.g., IndexedDB) should not need their own
  // additional checks once the StorageAccessHandle interface has been bound.
  bool has_full_cookie_access =
      GetContentClient()->browser()->IsFullCookieAccessAllowed(
          host->GetBrowserContext(), WebContents::FromRenderFrameHost(host),
          host->GetLastCommittedURL(), host->GetStorageKey());
  bool has_permission_access =
      host->GetProcess()
          ->GetBrowserContext()
          ->GetPermissionController()
          ->GetPermissionStatusForCurrentDocument(
              blink::PermissionType::STORAGE_ACCESS_GRANT, host) ==
      blink::mojom::PermissionStatus::GRANTED;
  if (!has_full_cookie_access && !has_permission_access) {
#if DCHECK_IS_ON()
    mojo::ReportBadMessage(
        "Binding a StorageAccessHandle requires third-party cookie access or "
        "permission access.");
#endif
    return;
  }
  new StorageAccessHandle(*host, std::move(receiver));
}

void StorageAccessHandle::BindIndexedDB(
    mojo::PendingReceiver<blink::mojom::IDBFactory> receiver) {
  render_frame_host().GetProcess()->BindIndexedDB(
      blink::StorageKey::CreateFirstParty(
          render_frame_host().GetStorageKey().origin()),
      static_cast<RenderFrameHostImpl&>(render_frame_host()),
      std::move(receiver));
}

void StorageAccessHandle::BindLocks(
    mojo::PendingReceiver<blink::mojom::LockManager> receiver) {
  render_frame_host().GetProcess()->CreateLockManager(
      blink::StorageKey::CreateFirstParty(
          render_frame_host().GetStorageKey().origin()),
      std::move(receiver));
}

void StorageAccessHandle::BindCaches(
    mojo::PendingReceiver<blink::mojom::CacheStorage> receiver) {
  RenderFrameHostImpl& host =
      static_cast<RenderFrameHostImpl&>(render_frame_host());
  mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
      coep_reporter_remote;
  if (host.coep_reporter()) {
    host.coep_reporter()->Clone(
        coep_reporter_remote.InitWithNewPipeAndPassReceiver());
  }
  host.GetProcess()->BindCacheStorage(
      host.cross_origin_embedder_policy(), std::move(coep_reporter_remote),
      host.policy_container_host()->policies().document_isolation_policy,
      storage::BucketLocator::ForDefaultBucket(
          blink::StorageKey::CreateFirstParty(host.GetStorageKey().origin())),
      std::move(receiver));
}

void StorageAccessHandle::GetDirectory(GetDirectoryCallback callback) {
  static_cast<RenderFrameHostImpl&>(render_frame_host())
      .GetStoragePartition()
      ->GetFileSystemAccessManager()
      ->GetSandboxedFileSystem(
          FileSystemAccessManagerImpl::BindingContext(
              blink::StorageKey::CreateFirstParty(
                  render_frame_host().GetStorageKey().origin()),
              render_frame_host().GetLastCommittedURL(),
              render_frame_host().GetGlobalId()),
          /*bucket=*/std::nullopt,
          /*directory_path_components=*/{}, std::move(callback));
}

void StorageAccessHandle::Estimate(EstimateCallback callback) {
  static_cast<RenderFrameHostImpl&>(render_frame_host())
      .GetStoragePartition()
      ->GetQuotaManagerProxy()
      ->GetBucketsForStorageKey(
          blink::StorageKey::CreateFirstParty(
              render_frame_host().GetStorageKey().origin()),
          blink::mojom::StorageType::kTemporary,
          /*delete_expired=*/false,
          base::SequencedTaskRunner::GetCurrentDefault(),
          base::BindOnce(&StorageAccessHandle::EstimateImpl,
                         weak_factory_.GetWeakPtr(), std::move(callback)));
}

void StorageAccessHandle::EstimateImpl(
    EstimateCallback callback,
    storage::QuotaErrorOr<std::set<storage::BucketInfo>> bucket_set) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!bucket_set.has_value()) {
    std::move(callback).Run(/*usage=*/0, /*quota=*/0, /*success=*/false);
    return;
  }
  storage::BucketInfo bucket_info;
  for (const storage::BucketInfo& info : *bucket_set) {
    if (info.is_default()) {
      bucket_info = info;
      break;
    }
  }
  if (bucket_info.is_null()) {
    std::move(callback).Run(/*usage=*/0, /*quota=*/0, /*success=*/true);
    return;
  }
  static_cast<RenderFrameHostImpl&>(render_frame_host())
      .GetStoragePartition()
      ->GetQuotaManagerProxy()
      ->GetBucketUsageAndQuota(
          bucket_info.id, base::SequencedTaskRunner::GetCurrentDefault(),
          base::BindOnce(&EstimateImplAfterGetBucketUsageAndQuota,
                         std::move(callback)));
}

void StorageAccessHandle::BindBlobStorage(
    mojo::PendingAssociatedReceiver<blink::mojom::BlobURLStore> receiver) {
  static_cast<RenderFrameHostImpl&>(render_frame_host())
      .GetStoragePartition()
      ->GetBlobUrlRegistry()
      ->AddReceiver(blink::StorageKey::CreateFirstParty(
                        render_frame_host().GetStorageKey().origin()),
                    render_frame_host().GetLastCommittedOrigin(),
                    render_frame_host().GetProcess()->GetID(),
                    std::move(receiver));
}

void StorageAccessHandle::BindBroadcastChannel(
    mojo::PendingAssociatedReceiver<blink::mojom::BroadcastChannelProvider>
        receiver) {
  BroadcastChannelService* service =
      static_cast<RenderFrameHostImpl&>(render_frame_host())
          .GetStoragePartition()
          ->GetBroadcastChannelService();
  service->AddAssociatedReceiver(
      std::make_unique<BroadcastChannelProvider>(
          service, blink::StorageKey::CreateFirstParty(
                       render_frame_host().GetStorageKey().origin())),
      std::move(receiver));
}

void StorageAccessHandle::BindSharedWorker(
    mojo::PendingReceiver<blink::mojom::SharedWorkerConnector> receiver) {
  SharedWorkerConnectorImpl::Create(
      PassKey(), render_frame_host().GetGlobalId(),
      blink::StorageKey::CreateFirstParty(
          render_frame_host().GetStorageKey().origin()),
      std::move(receiver));
}

StorageAccessHandle::StorageAccessHandle(
    RenderFrameHost& host,
    mojo::PendingReceiver<blink::mojom::StorageAccessHandle> receiver)
    : DocumentService(host, std::move(receiver)) {}

StorageAccessHandle::~StorageAccessHandle() = default;

}  // namespace content

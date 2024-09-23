// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_host.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "content/browser/broadcast_channel/broadcast_channel_provider.h"
#include "content/browser/broadcast_channel/broadcast_channel_service.h"
#include "content/browser/buckets/bucket_manager.h"
#include "content/browser/code_cache/generated_code_cache_context.h"
#include "content/browser/file_system_access/file_system_access_error.h"
#include "content/browser/renderer_host/code_cache_host_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/service_worker/service_worker_consts.h"
#include "content/browser/service_worker/service_worker_container_host.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/webtransport/web_transport_connector_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_client.h"
#include "content/public/common/origin_util.h"
#include "mojo/public/cpp/bindings/message.h"
#include "storage/browser/blob/blob_url_store_impl.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_client.mojom.h"

namespace content {

ServiceWorkerHost::ServiceWorkerHost(
    mojo::PendingAssociatedReceiver<blink::mojom::ServiceWorkerContainerHost>
        host_receiver,
    ServiceWorkerVersion& version,
    base::WeakPtr<ServiceWorkerContextCore> context)
    : worker_process_id_(ChildProcessHost::kInvalidUniqueID),
      version_(&version),
      token_(blink::ServiceWorkerToken()),
      broker_(this),
      container_host_(
          std::make_unique<content::ServiceWorkerContainerHostForServiceWorker>(
              std::move(context),
              this,
              version_->script_url(),
              version_->key())),
      host_receiver_(container_host_.get(), std::move(host_receiver)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

ServiceWorkerHost::~ServiceWorkerHost() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Explicitly destroy the ServiceWorkerContainerHost to release
  // ServiceWorkerObjectHosts and ServiceWorkerRegistrationObjectHosts owned by
  // that. Otherwise, this destructor can trigger their Mojo connection error
  // handlers, which would call back into halfway destroyed |this|. This is
  // because they are associated with the ServiceWorker interface, which can be
  // destroyed while in this destructor (|version_|'s |event_dispatcher_|).
  // See https://crbug.com/854993.
  container_host_.reset();
}

void ServiceWorkerHost::CompleteStartWorkerPreparation(
    int process_id,
    mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker> broker_receiver,
    mojo::PendingRemote<service_manager::mojom::InterfaceProvider>
        interface_provider_remote) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(ChildProcessHost::kInvalidUniqueID, worker_process_id_);
  DCHECK_NE(ChildProcessHost::kInvalidUniqueID, process_id);
  worker_process_id_ = process_id;
  broker_receiver_.Bind(std::move(broker_receiver));
  remote_interfaces_.Bind(std::move(interface_provider_remote));
}

void ServiceWorkerHost::CreateWebTransportConnector(
    mojo::PendingReceiver<blink::mojom::WebTransportConnector> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<WebTransportConnectorImpl>(
          worker_process_id_, /*frame=*/nullptr, version_->key().origin(),
          GetNetworkAnonymizationKey()),
      std::move(receiver));
}

void ServiceWorkerHost::BindCacheStorage(
    mojo::PendingReceiver<blink::mojom::CacheStorage> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  version_->embedded_worker()->BindCacheStorage(
      std::move(receiver),
      storage::BucketLocator::ForDefaultBucket(version_->key()));
}

void ServiceWorkerHost::GetSandboxedFileSystemForBucket(
    const storage::BucketInfo& bucket,
    const std::vector<std::string>& directory_path_components,
    blink::mojom::FileSystemAccessManager::GetSandboxedFileSystemCallback
        callback) {
  auto* process = GetProcessHost();
  if (process) {
    process->GetSandboxedFileSystemForBucket(bucket.ToBucketLocator(),
                                             directory_path_components,
                                             std::move(callback));
  } else {
    std::move(callback).Run(
        file_system_access_error::FromStatus(
            blink::mojom::FileSystemAccessStatus::kInvalidState,
            "Process gone."),
        {});
  }
}

#if !BUILDFLAG(IS_ANDROID)
void ServiceWorkerHost::BindHidService(
    mojo::PendingReceiver<blink::mojom::HidService> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  version_->embedded_worker()->BindHidService(version_->key().origin(),
                                              std::move(receiver));
}
#endif

void ServiceWorkerHost::BindUsbService(
    mojo::PendingReceiver<blink::mojom::WebUsbService> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (container_host_->top_frame_origin().opaque()) {
    // Service worker should not be available to a window/worker client whose
    // origin is opaque according to Service Worker specification. However, this
    // can possibly be triggered by a compromised renderer, so reject it and
    // report a bad mojo message.
    mojo::ReportBadMessage(
        "WebUSB is not allowed for the service worker scope when the top-level "
        "frame has an opaque origin.");
    return;
  }
  version_->embedded_worker()->BindUsbService(
      container_host_->top_frame_origin(), std::move(receiver));
}

net::NetworkIsolationKey ServiceWorkerHost::GetNetworkIsolationKey() const {
  return version_->key().ToPartialNetIsolationInfo().network_isolation_key();
}

net::NetworkAnonymizationKey ServiceWorkerHost::GetNetworkAnonymizationKey()
    const {
  return version_->key()
      .ToPartialNetIsolationInfo()
      .network_anonymization_key();
}

const base::UnguessableToken& ServiceWorkerHost::GetReportingSource() const {
  return version_->reporting_source();
}

StoragePartition* ServiceWorkerHost::GetStoragePartition() const {
  // It is possible that the RenderProcessHost is gone but we receive a request
  // before we had the opportunity to Detach because the disconnect handler
  // wasn't run yet. In such cases it is is safe to ignore these messages since
  // we are about to stop the service worker.
  auto* process = GetProcessHost();

  if (process == nullptr) {
    return nullptr;
  }

  return process->GetStoragePartition();
}

void ServiceWorkerHost::CreateCodeCacheHost(
    mojo::PendingReceiver<blink::mojom::CodeCacheHost> receiver) {
  auto embedded_worker_status = version_->embedded_worker()->status();
  // Due to IPC races it is possible that we receive code cache host requests
  // when the worker is stopping. For ex:
  // 1) Browser starts trying to stop, sends the Stop() IPC.
  // 2) Renderer sends a CreateCodeCacheHost() IPC.
  // 3) Renderer gets the Stop() IPC and realize it should try to stop the
  // worker.
  // Given the worker is stopping it is safe to ignore these messages.
  if (embedded_worker_status == blink::EmbeddedWorkerStatus::kStopping) {
    return;
  }

  // Create a new CodeCacheHostImpl and bind it to the given receiver.
  StoragePartition* storage_partition = GetStoragePartition();
  if (!storage_partition) {
    return;
  }
  if (!code_cache_host_receivers_) {
    code_cache_host_receivers_ =
        std::make_unique<CodeCacheHostImpl::ReceiverSet>(
            storage_partition->GetGeneratedCodeCacheContext());
  }
  code_cache_host_receivers_->Add(version_->embedded_worker()->process_id(),
                                  GetNetworkIsolationKey(),
                                  GetBucketStorageKey(), std::move(receiver));
}

void ServiceWorkerHost::CreateBroadcastChannelProvider(
    mojo::PendingReceiver<blink::mojom::BroadcastChannelProvider> receiver) {
  auto* storage_partition_impl =
      static_cast<StoragePartitionImpl*>(GetStoragePartition());
  if (!storage_partition_impl) {
    return;
  }

  auto* broadcast_channel_service =
      storage_partition_impl->GetBroadcastChannelService();
  broadcast_channel_service->AddReceiver(
      std::make_unique<BroadcastChannelProvider>(broadcast_channel_service,
                                                 version()->key()),
      std::move(receiver));
}

void ServiceWorkerHost::CreateBlobUrlStoreProvider(
    mojo::PendingReceiver<blink::mojom::BlobURLStore> receiver) {
  auto* storage_partition_impl =
      static_cast<StoragePartitionImpl*>(GetStoragePartition());
  if (!storage_partition_impl) {
    return;
  }

  storage_partition_impl->GetBlobUrlRegistry()->AddReceiver(
      version()->key(), version()->key().origin(), GetProcessHost()->GetID(),
      std::move(receiver));
}

void ServiceWorkerHost::CreateBucketManagerHost(
    mojo::PendingReceiver<blink::mojom::BucketManagerHost> receiver) {
  static_cast<StoragePartitionImpl*>(GetStoragePartition())
      ->GetBucketManager()
      ->BindReceiver(GetWeakPtr(), std::move(receiver),
                     mojo::GetBadMessageCallback());
}

base::WeakPtr<ServiceWorkerHost> ServiceWorkerHost::GetWeakPtr() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return weak_factory_.GetWeakPtr();
}

void ServiceWorkerHost::ReportNoBinderForInterface(const std::string& error) {
  broker_receiver_.ReportBadMessage(error + " for the service worker scope");
}

blink::StorageKey ServiceWorkerHost::GetBucketStorageKey() {
  return version_->key();
}

blink::mojom::PermissionStatus ServiceWorkerHost::GetPermissionStatus(
    blink::PermissionType permission_type) {
  auto* process = GetProcessHost();

  if (!process) {
    return blink::mojom::PermissionStatus::DENIED;
  }

  return process->GetBrowserContext()
      ->GetPermissionController()
      ->GetPermissionStatusForWorker(permission_type, process,
                                     GetBucketStorageKey().origin());
}

void ServiceWorkerHost::BindCacheStorageForBucket(
    const storage::BucketInfo& bucket,
    mojo::PendingReceiver<blink::mojom::CacheStorage> receiver) {
  version_->embedded_worker()->BindCacheStorage(std::move(receiver),
                                                bucket.ToBucketLocator());
}

storage::BucketClientInfo ServiceWorkerHost::GetBucketClientInfo() const {
  return storage::BucketClientInfo{worker_process_id(), token()};
}

RenderProcessHost* ServiceWorkerHost::GetProcessHost() const {
  return RenderProcessHost::FromID(version_->embedded_worker()->process_id());
}

void ServiceWorkerHost::BindAIManager(
    mojo::PendingReceiver<blink::mojom::AIManager> receiver) {
  auto* process = GetProcessHost();
  if (process) {
    GetContentClient()->browser()->BindAIManager(
        process->GetBrowserContext(),
        static_cast<base::SupportsUserData*>(this), std::move(receiver));
  }
}

}  // namespace content

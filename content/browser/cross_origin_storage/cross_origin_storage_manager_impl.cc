// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cross_origin_storage/cross_origin_storage_manager_impl.h"

#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/memory/self_deleting.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/cross_origin_storage/cross_origin_storage_file_handle_impl.h"
#include "content/browser/file_system_access/file_system_access_error.h"
#include "content/browser/file_system_access/file_system_access_manager_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/document_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_partition_config.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/is_potentially_trustworthy.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_error.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_file_handle.mojom.h"

namespace content {

namespace {

const void* const kWorkerManagerUserDataKey = &kWorkerManagerUserDataKey;

// Validates process security policy and storage partition eligibility, and
// constructs a `CrossOriginStorageManagerImpl` on success.
std::unique_ptr<CrossOriginStorageManagerImpl> CreateManagerIfAllowed(
    RenderProcessHost* process_host,
    StoragePartition* storage_partition,
    const blink::StorageKey& storage_key) {
  if (!process_host || !storage_partition) {
    return nullptr;
  }

  if (!ChildProcessSecurityPolicy::GetInstance()->CanAccessDataForOrigin(
          process_host->GetDeprecatedID(), storage_key.origin())) {
    return nullptr;
  }

  const StoragePartitionConfig& partition_config =
      storage_partition->GetConfig();
  if (partition_config.in_memory() || !partition_config.is_default()) {
    return nullptr;
  }

  auto* storage_partition_impl =
      static_cast<StoragePartitionImpl*>(storage_partition);
  BrowserContext* browser_context = storage_partition_impl->browser_context();
  if (!browser_context) {
    return nullptr;
  }

  auto* registry =
      CrossOriginStorageRegistry::GetOrCreateForBrowserContext(browser_context);
  if (!registry) {
    return nullptr;
  }

  base::WeakPtr<FileSystemAccessManagerImpl> file_system_access_manager =
      storage_partition_impl->GetFileSystemAccessManager()
          ? storage_partition_impl->GetFileSystemAccessManager()->AsWeakPtr()
          : nullptr;

  return std::make_unique<CrossOriginStorageManagerImpl>(
      registry->GetWeakPtr(),
      base::WrapRefCounted(ChromeBlobStorageContext::GetFor(browser_context)),
      std::move(file_system_access_manager), storage_key);
}

// Document-scoped wrapper so frame bindings do not outlive the document across
// navigations.
class FrameCrossOriginStorageManager final
    : public DocumentService<blink::mojom::CrossOriginStorageManager> {
 public:
  FrameCrossOriginStorageManager(
      RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<blink::mojom::CrossOriginStorageManager> receiver,
      std::unique_ptr<CrossOriginStorageManagerImpl> manager,
      base::SelfDeletingPassKey pass_key)
      : DocumentService(render_frame_host, std::move(receiver), pass_key),
        manager_(std::move(manager)) {}

  // blink::mojom::CrossOriginStorageManager:
  void GetFileHandle(blink::mojom::CrossOriginStorageHashPtr hash,
                     blink::mojom::CrossOriginStorageOriginsPtr create_origins,
                     GetFileHandleCallback callback) override {
    if (std::optional<std::string_view> bad_message =
            manager_->HandleGetFileHandle(
                std::move(hash), std::move(create_origins), callback)) {
      ReportBadMessageAndDeleteThis(*bad_message);
    }
  }

 private:
  ~FrameCrossOriginStorageManager() override = default;

  std::unique_ptr<CrossOriginStorageManagerImpl> manager_;
};

}  // namespace

// static
void CrossOriginStorageManagerImpl::BindForFrame(
    RenderFrameHost* frame_host,
    mojo::PendingReceiver<blink::mojom::CrossOriginStorageManager> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!frame_host) {
    return;
  }

  std::unique_ptr<CrossOriginStorageManagerImpl> manager =
      CreateManagerIfAllowed(frame_host->GetProcess(),
                             frame_host->GetStoragePartition(),
                             frame_host->GetStorageKey());
  if (!manager) {
    return;
  }

  base::MakeSelfDeleting<FrameCrossOriginStorageManager>(
      *frame_host, std::move(receiver), std::move(manager));
}

// static
void CrossOriginStorageManagerImpl::BindForWorker(
    base::SupportsUserData* worker_host,
    RenderProcessHost* process_host,
    const blink::StorageKey& storage_key,
    mojo::PendingReceiver<blink::mojom::CrossOriginStorageManager> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!worker_host || !process_host) {
    return;
  }

  auto* existing_manager = static_cast<CrossOriginStorageManagerImpl*>(
      worker_host->GetUserData(kWorkerManagerUserDataKey));
  if (existing_manager) {
    if (!ChildProcessSecurityPolicy::GetInstance()->CanAccessDataForOrigin(
            process_host->GetDeprecatedID(), storage_key.origin())) {
      return;
    }
    existing_manager->AddReceiver(std::move(receiver));
    return;
  }

  std::unique_ptr<CrossOriginStorageManagerImpl> manager =
      CreateManagerIfAllowed(process_host, process_host->GetStoragePartition(),
                             storage_key);
  if (!manager) {
    return;
  }

  manager->AddReceiver(std::move(receiver));
  worker_host->SetUserData(kWorkerManagerUserDataKey, std::move(manager));
}

CrossOriginStorageManagerImpl::CrossOriginStorageManagerImpl(
    base::WeakPtr<CrossOriginStorageRegistry> registry,
    scoped_refptr<ChromeBlobStorageContext> blob_storage_context,
    base::WeakPtr<FileSystemAccessManagerImpl> file_system_access_manager,
    blink::StorageKey storage_key)
    : registry_(std::move(registry)),
      blob_storage_context_(std::move(blob_storage_context)),
      file_system_access_manager_(std::move(file_system_access_manager)),
      storage_key_(std::move(storage_key)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

CrossOriginStorageManagerImpl::~CrossOriginStorageManagerImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void CrossOriginStorageManagerImpl::AddReceiver(
    mojo::PendingReceiver<blink::mojom::CrossOriginStorageManager> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  receivers_.Add(this, std::move(receiver));
}

std::optional<std::string_view>
CrossOriginStorageManagerImpl::HandleGetFileHandle(
    blink::mojom::CrossOriginStorageHashPtr hash,
    blink::mojom::CrossOriginStorageOriginsPtr create_origins,
    GetFileHandleCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Reject nonce-partitioned contexts (such as credentialless iframes and
  // fenced frames), third-party partitioned contexts (matching the spec's
  // default `'self'` Permissions Policy), and opaque or untrustworthy origins.
  // TODO(crbug.com/547302825): Support third-party `StorageKey`s when the
  // `cross-origin-storage` Permissions Policy feature is delegated and
  // `CrossOriginStorageRegistry` keys grants by
  // `storage_key_.top_level_site()`.
  const url::Origin& origin = storage_key_.origin();
  if (storage_key_.ForbidsUnpartitionedStorageAccess() ||
      storage_key_.IsThirdPartyContext() || origin.opaque() ||
      !origin.GetURL().SchemeIsHTTPOrHTTPS() ||
      !net::IsOriginPotentiallyTrustworthy(origin)) {
    std::move(callback).Run(
        file_system_access_error::FromStatus(
            blink::mojom::FileSystemAccessStatus::kPermissionDenied),
        mojo::NullRemote());
    return std::nullopt;
  }

  if (!hash) {
    return "Missing CrossOriginStorage hash";
  }

  std::optional<CrossOriginStorageHash> parsed_hash =
      CrossOriginStorageHash::Create(hash->algorithm, hash->value);
  if (!parsed_hash) {
    return "Invalid CrossOriginStorage hash";
  }

  if (!registry_ || !blob_storage_context_) {
    std::move(callback).Run(
        file_system_access_error::FromStatus(
            blink::mojom::FileSystemAccessStatus::kOperationFailed),
        mojo::NullRemote());
    return std::nullopt;
  }

  if (create_origins) {
    std::optional<CrossOriginStorageAccessSpec> parsed_spec =
        CrossOriginStorageAccessSpec::FromMojo(*create_origins);
    if (!parsed_spec) {
      return "Invalid CrossOriginStorage access spec";
    }

    mojo::PendingRemote<blink::mojom::FileSystemAccessFileHandle>
        file_handle_remote;
    mojo::MakeSelfOwnedReceiver(
        std::make_unique<CrossOriginStorageFileHandleImpl>(
            registry_, blob_storage_context_, file_system_access_manager_,
            std::move(*parsed_hash), origin, std::move(*parsed_spec),
            /*is_writable=*/true, /*may_read=*/false),
        file_handle_remote.InitWithNewPipeAndPassReceiver());

    std::move(callback).Run(file_system_access_error::Ok(),
                            std::move(file_handle_remote));
    return std::nullopt;
  }

  registry_->OpenForRead(
      *parsed_hash, origin, /*update_access_time=*/true,
      base::BindOnce(&CrossOriginStorageManagerImpl::OnOpenedForRead,
                     weak_factory_.GetWeakPtr(), *parsed_hash,
                     std::move(callback)));
  return std::nullopt;
}

void CrossOriginStorageManagerImpl::GetFileHandle(
    blink::mojom::CrossOriginStorageHashPtr hash,
    blink::mojom::CrossOriginStorageOriginsPtr create_origins,
    GetFileHandleCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (std::optional<std::string_view> bad_message = HandleGetFileHandle(
          std::move(hash), std::move(create_origins), callback)) {
    if (!receivers_.empty()) {
      receivers_.ReportBadMessage(*bad_message);
    } else {
      mojo::ReportBadMessage(*bad_message);
    }
  }
}

void CrossOriginStorageManagerImpl::OnOpenedForRead(
    CrossOriginStorageHash hash,
    GetFileHandleCallback callback,
    CrossOriginStorageOpenResult result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (result.status != CrossOriginStorageStatus::kSuccess) {
    std::move(callback).Run(
        file_system_access_error::FromFileError(
            result.status == CrossOriginStorageStatus::kNotFound
                ? base::File::FILE_ERROR_NOT_FOUND
                : base::File::FILE_ERROR_FAILED),
        mojo::NullRemote());
    return;
  }

  mojo::PendingRemote<blink::mojom::FileSystemAccessFileHandle>
      file_handle_remote;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<CrossOriginStorageFileHandleImpl>(
          registry_, blob_storage_context_, file_system_access_manager_,
          std::move(hash), storage_key_.origin(),
          CrossOriginStorageAccessSpec::AllowedOrigins({}),
          /*is_writable=*/false, /*may_read=*/true),
      file_handle_remote.InitWithNewPipeAndPassReceiver());

  std::move(callback).Run(file_system_access_error::Ok(),
                          std::move(file_handle_remote));
}

}  // namespace content

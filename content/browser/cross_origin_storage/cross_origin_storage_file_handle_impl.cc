// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cross_origin_storage/cross_origin_storage_file_handle_impl.h"

#include <memory>
#include <utility>

#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/uuid.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/cross_origin_storage/cross_origin_storage_file_writer_impl.h"
#include "content/browser/cross_origin_storage/cross_origin_storage_registry.h"
#include "content/browser/cross_origin_storage/cross_origin_storage_staging_writer.h"
#include "content/browser/file_system_access/file_system_access_error.h"
#include "content/browser/file_system_access/file_system_access_manager_impl.h"
#include "content/browser/file_system_access/file_system_access_transfer_token_impl.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_impl.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "third_party/blink/public/mojom/blob/serialized_blob.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_cloud_identifier.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_error.mojom.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"

namespace content {

namespace {

constexpr char kBlobContentType[] = "application/octet-stream";

void CreateBlobOnIOThread(scoped_refptr<ChromeBlobStorageContext> blob_context,
                          mojo::PendingReceiver<blink::mojom::Blob> receiver,
                          base::FilePath path,
                          std::string uuid,
                          uint64_t size) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!blob_context || !blob_context->context()) {
    return;
  }

  auto builder = std::make_unique<storage::BlobDataBuilder>(uuid);
  builder->set_content_type(kBlobContentType);
  if (size > 0) {
    builder->AppendFile(path, /*offset=*/0, size,
                        /*expected_modification_time=*/base::Time());
  }

  std::unique_ptr<storage::BlobDataHandle> handle =
      blob_context->context()->AddFinishedBlob(std::move(builder));
  if (!handle) {
    return;
  }
  storage::BlobImpl::Create(std::move(handle), std::move(receiver));
}

}  // namespace
CrossOriginStorageFileHandleImpl::CrossOriginStorageFileHandleImpl(
    base::WeakPtr<CrossOriginStorageRegistry> registry,
    scoped_refptr<ChromeBlobStorageContext> blob_storage_context,
    base::WeakPtr<FileSystemAccessManagerImpl> file_system_access_manager,
    CrossOriginStorageHash hash,
    url::Origin origin,
    CrossOriginStorageAccessSpec access_spec,
    bool is_writable,
    bool may_read)
    : registry_(std::move(registry)),
      blob_storage_context_(std::move(blob_storage_context)),
      file_system_access_manager_(std::move(file_system_access_manager)),
      hash_(std::move(hash)),
      origin_(std::move(origin)),
      access_spec_(std::move(access_spec)),
      is_writable_(is_writable),
      may_read_(may_read) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

CrossOriginStorageFileHandleImpl::~CrossOriginStorageFileHandleImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void CrossOriginStorageFileHandleImpl::GetPermissionStatus(
    blink::mojom::FileSystemAccessPermissionMode mode,
    GetPermissionStatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Read permission is always GRANTED. Write permission reports GRANTED if the
  // handle was created with `{create: true}` (`is_writable_`), DENIED
  // otherwise.
  if (mode != blink::mojom::FileSystemAccessPermissionMode::kRead &&
      !is_writable_) {
    std::move(callback).Run(blink::mojom::PermissionStatus::DENIED);
    return;
  }
  std::move(callback).Run(blink::mojom::PermissionStatus::GRANTED);
}

void CrossOriginStorageFileHandleImpl::RequestPermission(
    blink::mojom::FileSystemAccessPermissionMode mode,
    RequestPermissionCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Never prompt. Return status kOk alongside the permission status so the JS
  // promise resolves with the state ("granted" or "denied") rather than
  // rejecting.
  blink::mojom::PermissionStatus status =
      (mode != blink::mojom::FileSystemAccessPermissionMode::kRead &&
       !is_writable_)
          ? blink::mojom::PermissionStatus::DENIED
          : blink::mojom::PermissionStatus::GRANTED;
  std::move(callback).Run(file_system_access_error::Ok(), status);
}

void CrossOriginStorageFileHandleImpl::AsBlob(AsBlobCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Calling `getFile()` on an unwritten create handle must reject with
  // NotAllowedError so `{create: true}` cannot act as an existence oracle.
  if (!may_read_) {
    std::move(callback).Run(
        file_system_access_error::FromStatus(
            blink::mojom::FileSystemAccessStatus::kPermissionDenied),
        base::File::Info(), nullptr);
    return;
  }

  if (!registry_ || !blob_storage_context_) {
    std::move(callback).Run(
        file_system_access_error::FromStatus(
            blink::mojom::FileSystemAccessStatus::kOperationFailed),
        base::File::Info(), nullptr);
    return;
  }

  registry_->OpenForRead(
      hash_, origin_, /*update_access_time=*/false,
      base::BindOnce(&CrossOriginStorageFileHandleImpl::OnOpenedForRead,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void CrossOriginStorageFileHandleImpl::CreateFileWriter(
    bool keep_existing_data,
    bool /*auto_close*/,
    blink::mojom::FileSystemAccessWritableFileStreamLockMode /*mode*/,
    CreateFileWriterCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!is_writable_) {
    std::move(callback).Run(
        file_system_access_error::FromStatus(
            blink::mojom::FileSystemAccessStatus::kPermissionDenied,
            "Cannot create a writable stream on a read-only handle."),
        mojo::NullRemote());
    return;
  }

  // Per WICG § 4.3, `createWritable()` must produce a stream whose contents
  // start empty, regardless of the value of `keepExistingData`.

  if (!registry_) {
    std::move(callback).Run(
        file_system_access_error::FromStatus(
            blink::mojom::FileSystemAccessStatus::kOperationFailed),
        mojo::NullRemote());
    return;
  }

  registry_->CreateStagingWriter(
      hash_.algorithm(),
      base::BindOnce(&CrossOriginStorageFileHandleImpl::OnStagingWriterCreated,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void CrossOriginStorageFileHandleImpl::OnStagingWriterCreated(
    CreateFileWriterCallback callback,
    base::SequenceBound<CrossOriginStorageStagingWriter> staging_writer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!staging_writer) {
    std::move(callback).Run(
        file_system_access_error::FromStatus(
            blink::mojom::FileSystemAccessStatus::kOperationFailed,
            "Failed to create staging file."),
        mojo::NullRemote());
    return;
  }

  mojo::PendingRemote<blink::mojom::FileSystemAccessFileWriter> writer_remote;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<CrossOriginStorageFileWriterImpl>(
          registry_, hash_, origin_, access_spec_, std::move(staging_writer),
          base::BindOnce(&CrossOriginStorageFileHandleImpl::OnWriteCommitted,
                         weak_factory_.GetWeakPtr())),
      writer_remote.InitWithNewPipeAndPassReceiver());

  std::move(callback).Run(file_system_access_error::Ok(),
                          std::move(writer_remote));
}

void CrossOriginStorageFileHandleImpl::OnWriteCommitted() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  may_read_ = true;
}

void CrossOriginStorageFileHandleImpl::OnOpenedForRead(
    AsBlobCallback callback,
    CrossOriginStorageOpenResult result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (result.status != CrossOriginStorageStatus::kSuccess) {
    std::move(callback).Run(
        file_system_access_error::FromFileError(
            result.status == CrossOriginStorageStatus::kNotFound
                ? base::File::FILE_ERROR_NOT_FOUND
                : base::File::FILE_ERROR_FAILED),
        base::File::Info(), nullptr);
    return;
  }

  if (result.size < 0) {
    std::move(callback).Run(
        file_system_access_error::FromFileError(base::File::FILE_ERROR_FAILED),
        base::File::Info(), nullptr);
    return;
  }

  mojo::PendingRemote<blink::mojom::Blob> blob_remote;
  mojo::PendingReceiver<blink::mojom::Blob> blob_receiver =
      blob_remote.InitWithNewPipeAndPassReceiver();
  std::string uuid = base::Uuid::GenerateRandomV4().AsLowercaseString();

  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&CreateBlobOnIOThread, blob_storage_context_,
                                std::move(blob_receiver), result.path, uuid,
                                static_cast<uint64_t>(result.size)));

  base::File::Info info;
  info.size = result.size;
  info.is_directory = false;
  // Do not expose the on-disk modification timestamp across origins.
  info.last_modified = base::Time::UnixEpoch();

  std::move(callback).Run(
      file_system_access_error::Ok(), info,
      blink::mojom::SerializedBlob::New(uuid, kBlobContentType, result.size,
                                        std::move(blob_remote)));
}

void CrossOriginStorageFileHandleImpl::Move(
    mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken>
    /*destination_directory*/,
    const std::string& /*new_entry_name*/,
    MoveCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::move(callback).Run(file_system_access_error::FromStatus(
      blink::mojom::FileSystemAccessStatus::kPermissionDenied));
}

void CrossOriginStorageFileHandleImpl::Rename(
    const std::string& /*new_entry_name*/,
    RenameCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::move(callback).Run(file_system_access_error::FromStatus(
      blink::mojom::FileSystemAccessStatus::kPermissionDenied));
}

void CrossOriginStorageFileHandleImpl::Remove(RemoveCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::move(callback).Run(file_system_access_error::FromStatus(
      blink::mojom::FileSystemAccessStatus::kPermissionDenied));
}

void CrossOriginStorageFileHandleImpl::OpenAccessHandle(
    blink::mojom::FileSystemAccessAccessHandleLockMode /*mode*/,
    OpenAccessHandleCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // `createSyncAccessHandle()` rejects with InvalidStateError whenever the
  // handle is not in a bucket file system (WICG § 4.1). Hard-blocked so a
  // renderer can never obtain a direct OS file handle to a shared COS file.
  std::move(callback).Run(
      file_system_access_error::FromStatus(
          blink::mojom::FileSystemAccessStatus::kInvalidState),
      blink::mojom::FileSystemAccessAccessHandleFilePtr(), mojo::NullRemote());
}

void CrossOriginStorageFileHandleImpl::IsSameEntry(
    mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> other,
    IsSameEntryCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!other.is_valid() || !file_system_access_manager_) {
    std::move(callback).Run(
        file_system_access_error::FromStatus(
            blink::mojom::FileSystemAccessStatus::kPermissionDenied),
        false);
    return;
  }

  file_system_access_manager_->ResolveTransferToken(
      std::move(other),
      base::BindOnce(&CrossOriginStorageFileHandleImpl::IsSameEntryImpl,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void CrossOriginStorageFileHandleImpl::IsSameEntryImpl(
    IsSameEntryCallback callback,
    FileSystemAccessTransferTokenImpl* other) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!other || !other->cross_origin_storage_data().has_value() ||
      other->origin() != origin_) {
    // Spec § 4.1: isSameEntry() with an argument addressing a file system other
    // than Cross-Origin Storage, or an argument the calling origin did not
    // obtain, must reject with NotAllowedError.
    std::move(callback).Run(
        file_system_access_error::FromStatus(
            blink::mojom::FileSystemAccessStatus::kPermissionDenied),
        false);
    return;
  }

  // Spec § 4.1: Both handles address Cross-Origin Storage and were obtained
  // by the same origin. Return true if their hashes match, false otherwise.
  std::move(callback).Run(file_system_access_error::Ok(),
                          hash_ == other->cross_origin_storage_data()->hash);
}

void CrossOriginStorageFileHandleImpl::Transfer(
    mojo::PendingReceiver<blink::mojom::FileSystemAccessTransferToken> token) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!token.is_valid() || !file_system_access_manager_) {
    return;
  }
  file_system_access_manager_->CreateTransferToken(*this, std::move(token));
}

void CrossOriginStorageFileHandleImpl::GetUniqueId(
    GetUniqueIdCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::move(callback).Run(
      file_system_access_error::FromStatus(
          blink::mojom::FileSystemAccessStatus::kNotSupportedError),
      std::string());
}

void CrossOriginStorageFileHandleImpl::GetCloudIdentifiers(
    GetCloudIdentifiersCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::move(callback).Run(
      file_system_access_error::FromStatus(
          blink::mojom::FileSystemAccessStatus::kNotSupportedError),
      {});
}

}  // namespace content

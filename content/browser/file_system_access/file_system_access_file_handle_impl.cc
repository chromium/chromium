// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_file_handle_impl.h"

#include "base/guid.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "content/browser/file_system_access/file_system_access_error.h"
#include "content/browser/file_system_access/file_system_access_transfer_token_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/mime_util.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_impl.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "third_party/blink/public/mojom/blob/serialized_blob.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_error.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_transfer_token.mojom.h"

using blink::mojom::FileSystemAccessStatus;
using storage::BlobDataHandle;
using storage::BlobImpl;
using storage::FileSystemOperation;
using storage::FileSystemOperationRunner;
using storage::IsolatedContext;

namespace content {

FileSystemAccessFileHandleImpl::FileSystemAccessFileHandleImpl(
    FileSystemAccessManagerImpl* manager,
    const BindingContext& context,
    const storage::FileSystemURL& url,
    const SharedHandleState& handle_state)
    : FileSystemAccessHandleBase(manager, context, url, handle_state) {}

FileSystemAccessFileHandleImpl::~FileSystemAccessFileHandleImpl() = default;

void FileSystemAccessFileHandleImpl::GetPermissionStatus(
    bool writable,
    GetPermissionStatusCallback callback) {
  DoGetPermissionStatus(writable, std::move(callback));
}

void FileSystemAccessFileHandleImpl::RequestPermission(
    bool writable,
    RequestPermissionCallback callback) {
  DoRequestPermission(writable, std::move(callback));
}

void FileSystemAccessFileHandleImpl::AsBlob(AsBlobCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (GetReadPermissionStatus() != PermissionStatus::GRANTED) {
    std::move(callback).Run(file_system_access_error::FromStatus(
                                FileSystemAccessStatus::kPermissionDenied),
                            base::File::Info(), nullptr);
    return;
  }

  // TODO(mek): Check backend::SupportsStreaming and create snapshot file if
  // streaming is not supported.
  DoFileSystemOperation(
      FROM_HERE, &FileSystemOperationRunner::GetMetadata,
      base::BindOnce(&FileSystemAccessFileHandleImpl::DidGetMetaDataForBlob,
                     weak_factory_.GetWeakPtr(), std::move(callback)),
      url(),
      FileSystemOperation::GET_METADATA_FIELD_IS_DIRECTORY |
          FileSystemOperation::GET_METADATA_FIELD_SIZE |
          FileSystemOperation::GET_METADATA_FIELD_LAST_MODIFIED);
}

void FileSystemAccessFileHandleImpl::CreateFileWriter(
    bool keep_existing_data,
    bool auto_close,
    CreateFileWriterCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  RunWithWritePermission(
      base::BindOnce(&FileSystemAccessFileHandleImpl::CreateFileWriterImpl,
                     weak_factory_.GetWeakPtr(), keep_existing_data,
                     auto_close),
      base::BindOnce([](blink::mojom::FileSystemAccessErrorPtr result,
                        CreateFileWriterCallback callback) {
        std::move(callback).Run(std::move(result), mojo::NullRemote());
      }),
      std::move(callback));
}

void FileSystemAccessFileHandleImpl::IsSameEntry(
    mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> token,
    IsSameEntryCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  manager()->ResolveTransferToken(
      std::move(token),
      base::BindOnce(&FileSystemAccessFileHandleImpl::IsSameEntryImpl,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void FileSystemAccessFileHandleImpl::IsSameEntryImpl(
    IsSameEntryCallback callback,
    FileSystemAccessTransferTokenImpl* other) {
  if (!other) {
    std::move(callback).Run(
        file_system_access_error::FromStatus(
            blink::mojom::FileSystemAccessStatus::kOperationFailed),
        false);
    return;
  }

  if (other->type() != FileSystemAccessPermissionContext::HandleType::kFile) {
    std::move(callback).Run(file_system_access_error::Ok(), false);
    return;
  }

  const storage::FileSystemURL& url1 = url();
  const storage::FileSystemURL& url2 = other->url();

  // If two URLs are of a different type they are definitely not related.
  if (url1.type() != url2.type()) {
    std::move(callback).Run(file_system_access_error::Ok(), false);
    return;
  }

  // Otherwise compare path.
  const base::FilePath& path1 = url1.path();
  const base::FilePath& path2 = url2.path();
  std::move(callback).Run(file_system_access_error::Ok(), path1 == path2);
}

void FileSystemAccessFileHandleImpl::Transfer(
    mojo::PendingReceiver<blink::mojom::FileSystemAccessTransferToken> token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  manager()->CreateTransferToken(*this, std::move(token));
}

namespace {

void CreateBlobOnIOThread(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const scoped_refptr<ChromeBlobStorageContext>& blob_context,
    mojo::PendingReceiver<blink::mojom::Blob> blob_receiver,
    const storage::FileSystemURL& url,
    const std::string& blob_uuid,
    const std::string& content_type,
    const base::File::Info& info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto blob_builder = std::make_unique<storage::BlobDataBuilder>(blob_uuid);
  // Only append if the file has data.
  if (info.size > 0) {
    // Use AppendFileSystemFile here, since we're streaming the file directly
    // from the file system backend, and the file thus might not actually be
    // backed by a file on disk.
    blob_builder->AppendFileSystemFile(url, 0, info.size, info.last_modified,
                                       std::move(file_system_context));
  }
  blob_builder->set_content_type(content_type);

  std::unique_ptr<BlobDataHandle> blob_handle =
      blob_context->context()->AddFinishedBlob(std::move(blob_builder));

  // Since the blob we're creating doesn't depend on other blobs, and doesn't
  // require blob memory/disk quota, creating the blob can't fail.
  DCHECK(!blob_handle->IsBroken());

  BlobImpl::Create(std::move(blob_handle), std::move(blob_receiver));
}

}  // namespace

void FileSystemAccessFileHandleImpl::DidGetMetaDataForBlob(
    AsBlobCallback callback,
    base::File::Error result,
    const base::File::Info& info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (result != base::File::FILE_OK) {
    std::move(callback).Run(file_system_access_error::FromFileError(result),
                            base::File::Info(), nullptr);
    return;
  }

  std::string uuid = base::GenerateGUID();
  std::string content_type;

  base::FilePath::StringType extension = url().path().Extension();
  if (!extension.empty()) {
    std::string mime_type;
    // TODO(https://crbug.com/962306): Using GetMimeTypeFromExtension and
    // including platform defined mime type mappings might be nice/make sense,
    // however that method can potentially block and thus can't be called from
    // the IO thread.
    if (net::GetWellKnownMimeTypeFromExtension(extension.substr(1), &mime_type))
      content_type = std::move(mime_type);
  }
  // TODO(https://crbug.com/962306): Consider some kind of fallback type when
  // the above mime type detection fails.

  mojo::PendingRemote<blink::mojom::Blob> blob_remote;
  mojo::PendingReceiver<blink::mojom::Blob> blob_receiver =
      blob_remote.InitWithNewPipeAndPassReceiver();

  std::move(callback).Run(
      file_system_access_error::Ok(), info,
      blink::mojom::SerializedBlob::New(uuid, content_type, info.size,
                                        std::move(blob_remote)));

  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&CreateBlobOnIOThread,
                     base::WrapRefCounted(file_system_context()),
                     base::WrapRefCounted(manager()->blob_context()),
                     std::move(blob_receiver), url(), std::move(uuid),
                     std::move(content_type), info));
}

void FileSystemAccessFileHandleImpl::CreateFileWriterImpl(
    bool keep_existing_data,
    bool auto_close,
    CreateFileWriterCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(GetWritePermissionStatus(),
            blink::mojom::PermissionStatus::GRANTED);

  // We first attempt to create the swap file, even if we might do a subsequent
  // operation to copy a file to the same path if keep_existing_data is set.
  // This file creation has to be `exclusive`, meaning, it will fail if a file
  // already exists. Using the filesystem for synchronization, a successful
  // creation of the file ensures that this File Writer creation request owns
  // the file and eliminates possible race conditions.
  CreateSwapFile(
      /*count=*/0, keep_existing_data, auto_close, std::move(callback));
}

void FileSystemAccessFileHandleImpl::CreateSwapFile(
    int count,
    bool keep_existing_data,
    bool auto_close,
    CreateFileWriterCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(count >= 0);
  DCHECK(max_swap_files_ >= 0);

  if (GetWritePermissionStatus() != blink::mojom::PermissionStatus::GRANTED) {
    std::move(callback).Run(file_system_access_error::FromStatus(
                                FileSystemAccessStatus::kPermissionDenied),
                            mojo::NullRemote());
    return;
  }

  auto swap_path =
      base::FilePath(url().virtual_path()).AddExtensionASCII(".crswap");
  auto swap_file_system = file_system();

  if (count >= max_swap_files_) {
    DLOG(ERROR) << "Error Creating Swap File, count: " << count
                << " exceeds max unique files of: " << max_swap_files_
                << " base path: " << swap_path;
    std::move(callback).Run(file_system_access_error::FromStatus(
                                FileSystemAccessStatus::kOperationFailed,
                                "Failed to create swap file."),
                            mojo::NullRemote());
    return;
  }

  if (count > 0) {
    swap_path =
        swap_path.InsertBeforeExtensionASCII(base::StringPrintf(".%d", count));
  }

  // First attempt to just create the swap file in the same file system as this
  // file.
  storage::FileSystemURL swap_url =
      manager()->context()->CreateCrackedFileSystemURL(
          url().origin(), url().mount_type(), swap_path);

  // If that failed, it means this file was part of an isolated file system,
  // and specifically, a single file isolated file system. In that case we'll
  // need to register a new isolated file system for the swap file, and use that
  // for the writer.
  if (!swap_url.is_valid()) {
    DCHECK_EQ(url().mount_type(), storage::kFileSystemTypeIsolated);

    swap_path = base::FilePath(url().path()).AddExtensionASCII(".crswap");
    if (count > 0) {
      swap_path = swap_path.InsertBeforeExtensionASCII(
          base::StringPrintf(".%d", count));
    }

    auto handle = manager()->CreateFileSystemURLFromPath(
        context().origin, FileSystemAccessEntryFactory::PathType::kLocal,
        swap_path);
    swap_url = std::move(handle.url);
    swap_file_system = std::move(handle.file_system);
  }

  DoFileSystemOperation(
      FROM_HERE, &FileSystemOperationRunner::CreateFile,
      base::BindOnce(&FileSystemAccessFileHandleImpl::DidCreateSwapFile,
                     weak_factory_.GetWeakPtr(), count, swap_url,
                     swap_file_system, keep_existing_data, auto_close,
                     std::move(callback)),
      swap_url,
      /*exclusive=*/true);
}

void FileSystemAccessFileHandleImpl::DidCreateSwapFile(
    int count,
    const storage::FileSystemURL& swap_url,
    storage::IsolatedContext::ScopedFSHandle swap_file_system,
    bool keep_existing_data,
    bool auto_close,
    CreateFileWriterCallback callback,
    base::File::Error result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (result == base::File::FILE_ERROR_EXISTS) {
    // Creation attempt failed. We need to find an unused filename.
    CreateSwapFile(count + 1, keep_existing_data, auto_close,
                   std::move(callback));
    return;
  }

  if (result != base::File::FILE_OK) {
    DLOG(ERROR) << "Error Creating Swap File, status: "
                << base::File::ErrorToString(result)
                << " path: " << swap_url.path();
    std::move(callback).Run(file_system_access_error::FromFileError(
                                result, "Error creating swap file."),
                            mojo::NullRemote());
    return;
  }

  if (!keep_existing_data) {
    std::move(callback).Run(
        file_system_access_error::Ok(),
        manager()->CreateFileWriter(
            context(), url(), swap_url,
            FileSystemAccessManagerImpl::SharedHandleState(
                handle_state().read_grant, handle_state().write_grant,
                swap_file_system),
            auto_close));
    return;
  }

  DoFileSystemOperation(
      FROM_HERE, &FileSystemOperationRunner::Copy,
      base::BindOnce(&FileSystemAccessFileHandleImpl::DidCopySwapFile,
                     weak_factory_.GetWeakPtr(), swap_url, swap_file_system,
                     auto_close, std::move(callback)),
      url(), swap_url,
      storage::FileSystemOperation::OPTION_PRESERVE_LAST_MODIFIED,
      storage::FileSystemOperation::ERROR_BEHAVIOR_ABORT,
      storage::FileSystemOperation::CopyOrMoveProgressCallback());
}

void FileSystemAccessFileHandleImpl::DidCopySwapFile(
    const storage::FileSystemURL& swap_url,
    storage::IsolatedContext::ScopedFSHandle swap_file_system,
    bool auto_close,
    CreateFileWriterCallback callback,
    base::File::Error result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (result != base::File::FILE_OK) {
    DLOG(ERROR) << "Error Creating Swap File, status: "
                << base::File::ErrorToString(result)
                << " path: " << swap_url.path();
    std::move(callback).Run(file_system_access_error::FromFileError(
                                result, "Error copying to swap file."),
                            mojo::NullRemote());
    return;
  }
  std::move(callback).Run(file_system_access_error::Ok(),
                          manager()->CreateFileWriter(
                              context(), url(), swap_url,
                              FileSystemAccessManagerImpl::SharedHandleState(
                                  handle_state().read_grant,
                                  handle_state().write_grant, swap_file_system),
                              auto_close));
}

base::WeakPtr<FileSystemAccessHandleBase>
FileSystemAccessFileHandleImpl::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace content

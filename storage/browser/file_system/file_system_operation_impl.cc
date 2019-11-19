// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/file_system_operation_impl.h"

#include <stdint.h>

#include <limits>
#include <memory>
#include <tuple>
#include <utility>

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "net/base/escape.h"
#include "net/url_request/url_request.h"
#include "storage/browser/blob/shareable_file_reference.h"
#include "storage/browser/file_system/async_file_util.h"
#include "storage/browser/file_system/copy_or_move_operation_delegate.h"
#include "storage/browser/file_system/file_observers.h"
#include "storage/browser/file_system/file_system_backend.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_file_util.h"
#include "storage/browser/file_system/remove_operation_delegate.h"
#include "storage/browser/file_system/sandbox_file_system_backend.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/common/file_system/file_system_types.h"
#include "storage/common/file_system/file_system_util.h"

using storage::ScopedFile;

namespace storage {

namespace {

// Takes ownership and destruct on the target thread.
void Destruct(base::File file) {}

void DidOpenFile(scoped_refptr<FileSystemContext> context,
                 base::WeakPtr<FileSystemOperationImpl> operation,
                 FileSystemOperationImpl::OpenFileCallback callback,
                 base::File file,
                 base::OnceClosure on_close_callback) {
  if (!operation) {
    context->default_file_task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&Destruct, std::move(file)));
    return;
  }
  std::move(callback).Run(std::move(file), std::move(on_close_callback));
}

}  // namespace

FileSystemOperation* FileSystemOperation::Create(
    const FileSystemURL& url,
    FileSystemContext* file_system_context,
    std::unique_ptr<FileSystemOperationContext> operation_context) {
  return new FileSystemOperationImpl(url, file_system_context,
                                     std::move(operation_context));
}

FileSystemOperationImpl::~FileSystemOperationImpl() = default;

void FileSystemOperationImpl::CreateFile(const FileSystemURL& url,
                                         bool exclusive,
                                         StatusCallback callback) {
  DCHECK(SetPendingOperationType(kOperationCreateFile));

  auto repeatable_callback =
      base::AdaptCallbackForRepeating(std::move(callback));
  GetUsageAndQuotaThenRunTask(
      url,
      base::BindOnce(&FileSystemOperationImpl::DoCreateFile,
                     weak_factory_.GetWeakPtr(), url, repeatable_callback,
                     exclusive),
      base::BindOnce(repeatable_callback, base::File::FILE_ERROR_FAILED));
}

void FileSystemOperationImpl::CreateDirectory(const FileSystemURL& url,
                                              bool exclusive,
                                              bool recursive,
                                              StatusCallback callback) {
  DCHECK(SetPendingOperationType(kOperationCreateDirectory));

  auto repeatable_callback =
      base::AdaptCallbackForRepeating(std::move(callback));
  GetUsageAndQuotaThenRunTask(
      url,
      base::BindOnce(&FileSystemOperationImpl::DoCreateDirectory,
                     weak_factory_.GetWeakPtr(), url, repeatable_callback,
                     exclusive, recursive),
      base::BindOnce(repeatable_callback, base::File::FILE_ERROR_FAILED));
}

void FileSystemOperationImpl::Copy(
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    CopyOrMoveOption option,
    ErrorBehavior error_behavior,
    const CopyProgressCallback& progress_callback,
    StatusCallback callback) {
  DCHECK(SetPendingOperationType(kOperationCopy));
  DCHECK(!recursive_operation_delegate_);

  recursive_operation_delegate_.reset(new CopyOrMoveOperationDelegate(
      file_system_context(), src_url, dest_url,
      CopyOrMoveOperationDelegate::OPERATION_COPY, option, error_behavior,
      progress_callback,
      base::BindOnce(&FileSystemOperationImpl::DidFinishOperation,
                     weak_factory_.GetWeakPtr(), std::move(callback))));
  recursive_operation_delegate_->RunRecursively();
}

void FileSystemOperationImpl::Move(const FileSystemURL& src_url,
                                   const FileSystemURL& dest_url,
                                   CopyOrMoveOption option,
                                   StatusCallback callback) {
  DCHECK(SetPendingOperationType(kOperationMove));
  DCHECK(!recursive_operation_delegate_);
  recursive_operation_delegate_.reset(new CopyOrMoveOperationDelegate(
      file_system_context(), src_url, dest_url,
      CopyOrMoveOperationDelegate::OPERATION_MOVE, option, ERROR_BEHAVIOR_ABORT,
      FileSystemOperation::CopyProgressCallback(),
      base::BindOnce(&FileSystemOperationImpl::DidFinishOperation,
                     weak_factory_.GetWeakPtr(), std::move(callback))));
  recursive_operation_delegate_->RunRecursively();
}

void FileSystemOperationImpl::DirectoryExists(const FileSystemURL& url,
                                              StatusCallback callback) {
  DCHECK(SetPendingOperationType(kOperationDirectoryExists));
  async_file_util_->GetFileInfo(
      std::move(operation_context_), url, GET_METADATA_FIELD_IS_DIRECTORY,
      base::BindOnce(&FileSystemOperationImpl::DidDirectoryExists,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void FileSystemOperationImpl::FileExists(const FileSystemURL& url,
                                         StatusCallback callback) {
  DCHECK(SetPendingOperationType(kOperationFileExists));
  async_file_util_->GetFileInfo(
      std::move(operation_context_), url, GET_METADATA_FIELD_IS_DIRECTORY,
      base::BindOnce(&FileSystemOperationImpl::DidFileExists,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void FileSystemOperationImpl::GetMetadata(const FileSystemURL& url,
                                          int fields,
                                          GetMetadataCallback callback) {
  DCHECK(SetPendingOperationType(kOperationGetMetadata));
  async_file_util_->GetFileInfo(std::move(operation_context_), url, fields,
                                std::move(callback));
}

void FileSystemOperationImpl::ReadDirectory(
    const FileSystemURL& url,
    const ReadDirectoryCallback& callback) {
  DCHECK(SetPendingOperationType(kOperationReadDirectory));
  async_file_util_->ReadDirectory(std::move(operation_context_), url, callback);
}

void FileSystemOperationImpl::Remove(const FileSystemURL& url,
                                     bool recursive,
                                     StatusCallback callback) {
  DCHECK(SetPendingOperationType(kOperationRemove));
  DCHECK(!recursive_operation_delegate_);

  if (recursive) {
    // For recursive removal, try to delegate the operation to AsyncFileUtil
    // first. If not supported, it is delegated to RemoveOperationDelegate
    // in DidDeleteRecursively.
    async_file_util_->DeleteRecursively(
        std::move(operation_context_), url,
        base::BindOnce(&FileSystemOperationImpl::DidDeleteRecursively,
                       weak_factory_.GetWeakPtr(), url, std::move(callback)));
    return;
  }

  recursive_operation_delegate_.reset(new RemoveOperationDelegate(
      file_system_context(), url,
      base::BindOnce(&FileSystemOperationImpl::DidFinishOperation,
                     weak_factory_.GetWeakPtr(), std::move(callback))));
  recursive_operation_delegate_->Run();
}

void FileSystemOperationImpl::WriteBlob(
    const FileSystemURL& url,
    std::unique_ptr<FileWriterDelegate> writer_delegate,
    std::unique_ptr<BlobReader> blob_reader,
    const WriteCallback& callback) {
  DCHECK(SetPendingOperationType(kOperationWrite));
  file_writer_delegate_ = std::move(writer_delegate);
  file_writer_delegate_->Start(
      std::move(blob_reader),
      base::BindRepeating(&FileSystemOperationImpl::DidWrite,
                          weak_factory_.GetWeakPtr(), url, callback));
}

void FileSystemOperationImpl::Write(
    const FileSystemURL& url,
    std::unique_ptr<FileWriterDelegate> writer_delegate,
    mojo::ScopedDataPipeConsumerHandle data_pipe,
    const WriteCallback& callback) {
  DCHECK(SetPendingOperationType(kOperationWrite));
  file_writer_delegate_ = std::move(writer_delegate);
  file_writer_delegate_->Start(
      std::move(data_pipe),
      base::BindRepeating(&FileSystemOperationImpl::DidWrite,
                          weak_factory_.GetWeakPtr(), url, callback));
}

void FileSystemOperationImpl::Truncate(const FileSystemURL& url,
                                       int64_t length,
                                       StatusCallback callback) {
  DCHECK(SetPendingOperationType(kOperationTruncate));

  auto repeatable_callback =
      base::AdaptCallbackForRepeating(std::move(callback));
  GetUsageAndQuotaThenRunTask(
      url,
      base::BindOnce(&FileSystemOperationImpl::DoTruncate,
                     weak_factory_.GetWeakPtr(), url, repeatable_callback,
                     length),
      base::BindOnce(repeatable_callback, base::File::FILE_ERROR_FAILED));
}

void FileSystemOperationImpl::TouchFile(const FileSystemURL& url,
                                        const base::Time& last_access_time,
                                        const base::Time& last_modified_time,
                                        StatusCallback callback) {
  DCHECK(SetPendingOperationType(kOperationTouchFile));

  async_file_util_->Touch(
      std::move(operation_context_), url, last_access_time, last_modified_time,
      base::BindOnce(&FileSystemOperationImpl::DidFinishOperation,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void FileSystemOperationImpl::OpenFile(const FileSystemURL& url,
                                       int file_flags,
                                       OpenFileCallback callback) {
  DCHECK(SetPendingOperationType(kOperationOpenFile));

  if (file_flags & (base::File::FLAG_TEMPORARY | base::File::FLAG_HIDDEN)) {
    std::move(callback).Run(base::File(base::File::FILE_ERROR_FAILED),
                            base::OnceClosure());
    return;
  }

  auto repeatable_callback =
      base::AdaptCallbackForRepeating(std::move(callback));
  GetUsageAndQuotaThenRunTask(
      url,
      base::BindOnce(&FileSystemOperationImpl::DoOpenFile,
                     weak_factory_.GetWeakPtr(), url, repeatable_callback,
                     file_flags),
      base::BindOnce(repeatable_callback,
                     base::File(base::File::FILE_ERROR_FAILED),
                     base::OnceClosure()));
}

// We can only get here on a write or truncate that's not yet completed.
// We don't support cancelling any other operation at this time.
void FileSystemOperationImpl::Cancel(StatusCallback cancel_callback) {
  DCHECK(cancel_callback_.is_null());
  cancel_callback_ = std::move(cancel_callback);

  if (file_writer_delegate_.get()) {
    DCHECK_EQ(kOperationWrite, pending_operation_);
    // This will call DidWrite() with ABORT status code.
    file_writer_delegate_->Cancel();
  } else if (recursive_operation_delegate_) {
    // This will call DidFinishOperation() with ABORT status code.
    recursive_operation_delegate_->Cancel();
  } else {
    // For truncate we have no way to cancel the inflight operation (for now).
    // Let it just run and dispatch cancel callback later.
    DCHECK_EQ(kOperationTruncate, pending_operation_);
  }
}

void FileSystemOperationImpl::CreateSnapshotFile(
    const FileSystemURL& url,
    SnapshotFileCallback callback) {
  DCHECK(SetPendingOperationType(kOperationCreateSnapshotFile));
  async_file_util_->CreateSnapshotFile(std::move(operation_context_), url,
                                       std::move(callback));
}

void FileSystemOperationImpl::CopyInForeignFile(
    const base::FilePath& src_local_disk_file_path,
    const FileSystemURL& dest_url,
    StatusCallback callback) {
  DCHECK(SetPendingOperationType(kOperationCopyInForeignFile));

  auto repeatable_callback =
      base::AdaptCallbackForRepeating(std::move(callback));
  GetUsageAndQuotaThenRunTask(
      dest_url,
      base::BindOnce(&FileSystemOperationImpl::DoCopyInForeignFile,
                     weak_factory_.GetWeakPtr(), src_local_disk_file_path,
                     dest_url, repeatable_callback),
      base::BindOnce(repeatable_callback, base::File::FILE_ERROR_FAILED));
}

void FileSystemOperationImpl::RemoveFile(const FileSystemURL& url,
                                         StatusCallback callback) {
  DCHECK(SetPendingOperationType(kOperationRemove));
  async_file_util_->DeleteFile(
      std::move(operation_context_), url,
      base::BindOnce(&FileSystemOperationImpl::DidFinishOperation,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void FileSystemOperationImpl::RemoveDirectory(const FileSystemURL& url,
                                              StatusCallback callback) {
  DCHECK(SetPendingOperationType(kOperationRemove));
  async_file_util_->DeleteDirectory(
      std::move(operation_context_), url,
      base::BindOnce(&FileSystemOperationImpl::DidFinishOperation,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void FileSystemOperationImpl::CopyFileLocal(
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    CopyOrMoveOption option,
    const CopyFileProgressCallback& progress_callback,
    StatusCallback callback) {
  DCHECK(SetPendingOperationType(kOperationCopy));
  DCHECK(src_url.IsInSameFileSystem(dest_url));

  auto repeatable_callback =
      base::AdaptCallbackForRepeating(std::move(callback));
  GetUsageAndQuotaThenRunTask(
      dest_url,
      base::BindOnce(&FileSystemOperationImpl::DoCopyFileLocal,
                     weak_factory_.GetWeakPtr(), src_url, dest_url, option,
                     progress_callback, repeatable_callback),
      base::BindOnce(repeatable_callback, base::File::FILE_ERROR_FAILED));
}

void FileSystemOperationImpl::MoveFileLocal(const FileSystemURL& src_url,
                                            const FileSystemURL& dest_url,
                                            CopyOrMoveOption option,
                                            StatusCallback callback) {
  DCHECK(SetPendingOperationType(kOperationMove));
  DCHECK(src_url.IsInSameFileSystem(dest_url));

  auto repeatable_callback =
      base::AdaptCallbackForRepeating(std::move(callback));
  GetUsageAndQuotaThenRunTask(
      dest_url,
      base::BindOnce(&FileSystemOperationImpl::DoMoveFileLocal,
                     weak_factory_.GetWeakPtr(), src_url, dest_url, option,
                     repeatable_callback),
      base::BindOnce(repeatable_callback, base::File::FILE_ERROR_FAILED));
}

base::File::Error FileSystemOperationImpl::SyncGetPlatformPath(
    const FileSystemURL& url,
    base::FilePath* platform_path) {
  DCHECK(SetPendingOperationType(kOperationGetLocalPath));
  if (!file_system_context()->IsSandboxFileSystem(url.type()))
    return base::File::FILE_ERROR_INVALID_OPERATION;
  FileSystemFileUtil* file_util =
      file_system_context()->sandbox_delegate()->sync_file_util();
  file_util->GetLocalFilePath(operation_context_.get(), url, platform_path);
  return base::File::FILE_OK;
}

FileSystemOperationImpl::FileSystemOperationImpl(
    const FileSystemURL& url,
    FileSystemContext* file_system_context,
    std::unique_ptr<FileSystemOperationContext> operation_context)
    : file_system_context_(file_system_context),
      operation_context_(std::move(operation_context)),
      async_file_util_(nullptr),
      pending_operation_(kOperationNone) {
  weak_ptr_ = weak_factory_.GetWeakPtr();

  DCHECK(operation_context_.get());
  operation_context_->DetachFromSequence();
  async_file_util_ = file_system_context_->GetAsyncFileUtil(url.type());
  DCHECK(async_file_util_);
}

void FileSystemOperationImpl::GetUsageAndQuotaThenRunTask(
    const FileSystemURL& url,
    base::OnceClosure task,
    base::OnceClosure error_callback) {
  storage::QuotaManagerProxy* quota_manager_proxy =
      file_system_context()->quota_manager_proxy();
  if (!quota_manager_proxy ||
      !file_system_context()->GetQuotaUtil(url.type())) {
    // If we don't have the quota manager or the requested filesystem type
    // does not support quota, we should be able to let it go.
    operation_context_->set_allowed_bytes_growth(
        std::numeric_limits<int64_t>::max());
    std::move(task).Run();
    return;
  }

  DCHECK(quota_manager_proxy);
  DCHECK(quota_manager_proxy->quota_manager());
  quota_manager_proxy->quota_manager()->GetUsageAndQuota(
      url::Origin::Create(url.origin().GetURL()),
      FileSystemTypeToQuotaStorageType(url.type()),
      base::BindOnce(&FileSystemOperationImpl::DidGetUsageAndQuotaAndRunTask,
                     weak_ptr_, std::move(task), std::move(error_callback)));
}

void FileSystemOperationImpl::DidGetUsageAndQuotaAndRunTask(
    base::OnceClosure task,
    base::OnceClosure error_callback,
    blink::mojom::QuotaStatusCode status,
    int64_t usage,
    int64_t quota) {
  if (status != blink::mojom::QuotaStatusCode::kOk) {
    LOG(WARNING) << "Got unexpected quota error : " << static_cast<int>(status);
    std::move(error_callback).Run();
    return;
  }

  operation_context_->set_allowed_bytes_growth(quota - usage);
  std::move(task).Run();
}

void FileSystemOperationImpl::DoCreateFile(const FileSystemURL& url,
                                           StatusCallback callback,
                                           bool exclusive) {
  async_file_util_->EnsureFileExists(
      std::move(operation_context_), url,
      base::BindOnce(
          exclusive ? &FileSystemOperationImpl::DidEnsureFileExistsExclusive
                    : &FileSystemOperationImpl::DidEnsureFileExistsNonExclusive,
          weak_ptr_, std::move(callback)));
}

void FileSystemOperationImpl::DoCreateDirectory(const FileSystemURL& url,
                                                StatusCallback callback,
                                                bool exclusive,
                                                bool recursive) {
  async_file_util_->CreateDirectory(
      std::move(operation_context_), url, exclusive, recursive,
      base::BindOnce(&FileSystemOperationImpl::DidFinishOperation, weak_ptr_,
                     std::move(callback)));
}

void FileSystemOperationImpl::DoCopyFileLocal(
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    CopyOrMoveOption option,
    const CopyFileProgressCallback& progress_callback,
    StatusCallback callback) {
  async_file_util_->CopyFileLocal(
      std::move(operation_context_), src_url, dest_url, option,
      progress_callback,
      base::BindOnce(&FileSystemOperationImpl::DidFinishOperation, weak_ptr_,
                     std::move(callback)));
}

void FileSystemOperationImpl::DoMoveFileLocal(const FileSystemURL& src_url,
                                              const FileSystemURL& dest_url,
                                              CopyOrMoveOption option,
                                              StatusCallback callback) {
  async_file_util_->MoveFileLocal(
      std::move(operation_context_), src_url, dest_url, option,
      base::BindOnce(&FileSystemOperationImpl::DidFinishOperation, weak_ptr_,
                     std::move(callback)));
}

void FileSystemOperationImpl::DoCopyInForeignFile(
    const base::FilePath& src_local_disk_file_path,
    const FileSystemURL& dest_url,
    StatusCallback callback) {
  async_file_util_->CopyInForeignFile(
      std::move(operation_context_), src_local_disk_file_path, dest_url,
      base::BindOnce(&FileSystemOperationImpl::DidFinishOperation, weak_ptr_,
                     std::move(callback)));
}

void FileSystemOperationImpl::DoTruncate(const FileSystemURL& url,
                                         StatusCallback callback,
                                         int64_t length) {
  async_file_util_->Truncate(
      std::move(operation_context_), url, length,
      base::BindOnce(&FileSystemOperationImpl::DidFinishOperation, weak_ptr_,
                     std::move(callback)));
}

void FileSystemOperationImpl::DoOpenFile(const FileSystemURL& url,
                                         OpenFileCallback callback,
                                         int file_flags) {
  async_file_util_->CreateOrOpen(
      std::move(operation_context_), url, file_flags,
      base::BindOnce(&DidOpenFile, file_system_context_, weak_ptr_,
                     std::move(callback)));
}

void FileSystemOperationImpl::DidEnsureFileExistsExclusive(
    StatusCallback callback,
    base::File::Error rv,
    bool created) {
  if (rv == base::File::FILE_OK && !created) {
    std::move(callback).Run(base::File::FILE_ERROR_EXISTS);
  } else {
    DidFinishOperation(std::move(callback), rv);
  }
}

void FileSystemOperationImpl::DidEnsureFileExistsNonExclusive(
    StatusCallback callback,
    base::File::Error rv,
    bool /* created */) {
  DidFinishOperation(std::move(callback), rv);
}

void FileSystemOperationImpl::DidFinishOperation(StatusCallback callback,
                                                 base::File::Error rv) {
  if (!cancel_callback_.is_null()) {
    StatusCallback cancel_callback = std::move(cancel_callback_);
    std::move(callback).Run(rv);

    // Return OK only if we succeeded to stop the operation.
    std::move(cancel_callback)
        .Run(rv == base::File::FILE_ERROR_ABORT
                 ? base::File::FILE_OK
                 : base::File::FILE_ERROR_INVALID_OPERATION);
  } else {
    std::move(callback).Run(rv);
  }
}

void FileSystemOperationImpl::DidDirectoryExists(
    StatusCallback callback,
    base::File::Error rv,
    const base::File::Info& file_info) {
  if (rv == base::File::FILE_OK && !file_info.is_directory)
    rv = base::File::FILE_ERROR_NOT_A_DIRECTORY;
  std::move(callback).Run(rv);
}

void FileSystemOperationImpl::DidFileExists(StatusCallback callback,
                                            base::File::Error rv,
                                            const base::File::Info& file_info) {
  if (rv == base::File::FILE_OK && file_info.is_directory)
    rv = base::File::FILE_ERROR_NOT_A_FILE;
  std::move(callback).Run(rv);
}

void FileSystemOperationImpl::DidDeleteRecursively(const FileSystemURL& url,
                                                   StatusCallback callback,
                                                   base::File::Error rv) {
  if (rv == base::File::FILE_ERROR_INVALID_OPERATION) {
    // Recursive removal is not supported on this platform.
    DCHECK(!recursive_operation_delegate_);
    recursive_operation_delegate_.reset(new RemoveOperationDelegate(
        file_system_context(), url,
        base::BindOnce(&FileSystemOperationImpl::DidFinishOperation, weak_ptr_,
                       std::move(callback))));
    recursive_operation_delegate_->RunRecursively();
    return;
  }

  std::move(callback).Run(rv);
}

void FileSystemOperationImpl::DidWrite(
    const FileSystemURL& url,
    const WriteCallback& write_callback,
    base::File::Error rv,
    int64_t bytes,
    FileWriterDelegate::WriteProgressStatus write_status) {
  const bool complete =
      (write_status != FileWriterDelegate::SUCCESS_IO_PENDING);
  if (complete && write_status != FileWriterDelegate::ERROR_WRITE_NOT_STARTED) {
    DCHECK(operation_context_);
    operation_context_->change_observers()->Notify(
        &FileChangeObserver::OnModifyFile, url);
  }

  StatusCallback cancel_callback = std::move(cancel_callback_);
  write_callback.Run(rv, bytes, complete);
  if (!cancel_callback.is_null())
    std::move(cancel_callback).Run(base::File::FILE_OK);
}

bool FileSystemOperationImpl::SetPendingOperationType(OperationType type) {
  if (pending_operation_ != kOperationNone)
    return false;
  pending_operation_ = type;
  return true;
}

}  // namespace storage

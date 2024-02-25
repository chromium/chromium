// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/file_system_operation_runner.h"

#include <stdint.h>

#include <memory>
#include <tuple>
#include <utility>

#include "base/auto_reset.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "storage/browser/blob/shareable_file_reference.h"
#include "storage/browser/file_system/copy_or_move_hook_delegate.h"
#include "storage/browser/file_system/file_observers.h"
#include "storage/browser/file_system/file_stream_writer.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation.h"
#include "storage/browser/file_system/file_writer_delegate.h"

namespace storage {

using OperationID = FileSystemOperationRunner::OperationID;

FileSystemOperationRunner::FileSystemOperationRunner(
    base::PassKey<FileSystemContext>,
    const scoped_refptr<FileSystemContext>& file_system_context)
    : FileSystemOperationRunner(file_system_context.get()) {}

FileSystemOperationRunner::FileSystemOperationRunner(
    base::PassKey<FileSystemContext>,
    FileSystemContext* file_system_context)
    : FileSystemOperationRunner(file_system_context) {}

FileSystemOperationRunner::~FileSystemOperationRunner() = default;

void FileSystemOperationRunner::Shutdown() {
  // Clearing |operations_| may release our owning FileSystemContext, causing
  // |this| to be deleted, so do not touch |this| after clear()ing it.
  operations_.clear();
}

OperationID FileSystemOperationRunner::CreateFile(const FileSystemURL& url,
                                                  bool exclusive,
                                                  StatusCallback callback) {
  base::File::Error error = base::File::FILE_OK;
  std::unique_ptr<FileSystemOperation> operation =
      file_system_context_->CreateFileSystemOperation(
          OperationType::kCreateFile, url, &error);
  FileSystemOperation* operation_raw = operation.get();
  OperationID id = BeginOperation(std::move(operation));
  base::AutoReset<bool> beginning(&is_beginning_operation_, true);
  if (!operation_raw) {
    DidFinish(id, std::move(callback), error);
    return id;
  }
  PrepareForWrite(id, url);
  operation_raw->CreateFile(
      url, exclusive,
      base::BindOnce(&FileSystemOperationRunner::DidFinish, weak_ptr_, id,
                     std::move(callback)));
  return id;
}

OperationID FileSystemOperationRunner::CreateDirectory(
    const FileSystemURL& url,
    bool exclusive,
    bool recursive,
    StatusCallback callback) {
  base::File::Error error = base::File::FILE_OK;
  std::unique_ptr<FileSystemOperation> operation =
      file_system_context_->CreateFileSystemOperation(
          OperationType::kCreateDirectory, url, &error);
  FileSystemOperation* operation_raw = operation.get();
  OperationID id = BeginOperation(std::move(operation));
  base::AutoReset<bool> beginning(&is_beginning_operation_, true);
  if (!operation_raw) {
    DidFinish(id, std::move(callback), error);
    return id;
  }
  PrepareForWrite(id, url);
  operation_raw->CreateDirectory(
      url, exclusive, recursive,
      base::BindOnce(&FileSystemOperationRunner::DidFinish, weak_ptr_, id,
                     std::move(callback)));
  return id;
}

OperationID FileSystemOperationRunner::Copy(
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    CopyOrMoveOptionSet options,
    ErrorBehavior error_behavior,
    std::unique_ptr<CopyOrMoveHookDelegate> copy_or_move_hook_delegate,
    StatusCallback callback) {
  DCHECK(copy_or_move_hook_delegate);
  base::File::Error error = base::File::FILE_OK;
  std::unique_ptr<FileSystemOperation> operation =
      file_system_context_->CreateFileSystemOperation(OperationType::kCopy,
                                                      dest_url, &error);
  FileSystemOperation* operation_raw = operation.get();
  OperationID id = BeginOperation(std::move(operation));
  base::AutoReset<bool> beginning(&is_beginning_operation_, true);
  if (!operation_raw) {
    DidFinish(id, std::move(callback), error);
    return id;
  }
  PrepareForWrite(id, dest_url);
  PrepareForRead(id, src_url);
  operation_raw->Copy(src_url, dest_url, options, error_behavior,
                      std::move(copy_or_move_hook_delegate),
                      base::BindOnce(&FileSystemOperationRunner::DidFinish,
                                     weak_ptr_, id, std::move(callback)));
  return id;
}

OperationID FileSystemOperationRunner::Move(
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    CopyOrMoveOptionSet options,
    ErrorBehavior error_behavior,
    std::unique_ptr<CopyOrMoveHookDelegate> copy_or_move_hook_delegate,
    StatusCallback callback) {
  DCHECK(copy_or_move_hook_delegate);
  base::File::Error error = base::File::FILE_OK;
  std::unique_ptr<FileSystemOperation> operation =
      file_system_context_->CreateFileSystemOperation(OperationType::kMove,
                                                      dest_url, &error);
  FileSystemOperation* operation_raw = operation.get();
  OperationID id = BeginOperation(std::move(operation));
  base::AutoReset<bool> beginning(&is_beginning_operation_, true);
  if (!operation_raw) {
    DidFinish(id, std::move(callback), error);
    return id;
  }
  PrepareForWrite(id, dest_url);
  PrepareForWrite(id, src_url);
  operation_raw->Move(src_url, dest_url, options, error_behavior,
                      std::move(copy_or_move_hook_delegate),
                      base::BindOnce(&FileSystemOperationRunner::DidFinish,
                                     weak_ptr_, id, std::move(callback)));
  return id;
}

OperationID FileSystemOperationRunner::DirectoryExists(
    const FileSystemURL& url,
    StatusCallback callback) {
  base::File::Error error = base::File::FILE_OK;
  std::unique_ptr<FileSystemOperation> operation =
      file_system_context_->CreateFileSystemOperation(
          OperationType::kDirectoryExists, url, &error);
  FileSystemOperation* operation_raw = operation.get();
  OperationID id = BeginOperation(std::move(operation));
  base::AutoReset<bool> beginning(&is_beginning_operation_, true);
  if (!operation_raw) {
    DidFinish(id, std::move(callback), error);
    return id;
  }
  PrepareForRead(id, url);
  operation_raw->DirectoryExists(
      url, base::BindOnce(&FileSystemOperationRunner::DidFinish, weak_ptr_, id,
                          std::move(callback)));
  return id;
}

OperationID FileSystemOperationRunner::FileExists(const FileSystemURL& url,
                                                  StatusCallback callback) {
  base::File::Error error = base::File::FILE_OK;
  std::unique_ptr<FileSystemOperation> operation =
      file_system_context_->CreateFileSystemOperation(
          OperationType::kFileExists, url, &error);
  FileSystemOperation* operation_raw = operation.get();
  OperationID id = BeginOperation(std::move(operation));
  base::AutoReset<bool> beginning(&is_beginning_operation_, true);
  if (!operation_raw) {
    DidFinish(id, std::move(callback), error);
    return id;
  }
  PrepareForRead(id, url);
  operation_raw->FileExists(
      url, base::BindOnce(&FileSystemOperationRunner::DidFinish, weak_ptr_, id,
                          std::move(callback)));
  return id;
}

OperationID FileSystemOperationRunner::GetMetadata(
    const FileSystemURL& url,
    GetMetadataFieldSet fields,
    GetMetadataCallback callback) {
  base::File::Error error = base::File::FILE_OK;
  std::unique_ptr<FileSystemOperation> operation =
      file_system_context_->CreateFileSystemOperation(
          OperationType::kGetMetadata, url, &error);
  FileSystemOperation* operation_raw = operation.get();
  OperationID id = BeginOperation(std::move(operation));
  base::AutoReset<bool> beginning(&is_beginning_operation_, true);
  if (!operation_raw) {
    DidGetMetadata(id, std::move(callback), error, base::File::Info());
    return id;
  }
  PrepareForRead(id, url);
  operation_raw->GetMetadata(
      url, fields,
      base::BindOnce(&FileSystemOperationRunner::DidGetMetadata, weak_ptr_, id,
                     std::move(callback)));
  return id;
}

OperationID FileSystemOperationRunner::ReadDirectory(
    const FileSystemURL& url,
    const ReadDirectoryCallback& callback) {
  base::File::Error error = base::File::FILE_OK;
  std::unique_ptr<FileSystemOperation> operation =
      file_system_context_->CreateFileSystemOperation(
          OperationType::kReadDirectory, url, &error);
  FileSystemOperation* operation_raw = operation.get();
  OperationID id = BeginOperation(std::move(operation));
  base::AutoReset<bool> beginning(&is_beginning_operation_, true);
  if (!operation_raw) {
    DidReadDirectory(id, std::move(callback), error,
                     std::vector<filesystem::mojom::DirectoryEntry>(), false);
    return id;
  }
  PrepareForRead(id, url);
  operation_raw->ReadDirectory(
      url, base::BindRepeating(&FileSystemOperationRunner::DidReadDirectory,
                               weak_ptr_, id, callback));
  return id;
}

OperationID FileSystemOperationRunner::Remove(const FileSystemURL& url,
                                              bool recursive,
                                              StatusCallback callback) {
  base::File::Error error = base::File::FILE_OK;
  std::unique_ptr<FileSystemOperation> operation =
      file_system_context_->CreateFileSystemOperation(OperationType::kRemove,
                                                      url, &error);
  FileSystemOperation* operation_raw = operation.get();
  OperationID id = BeginOperation(std::move(operation));
  base::AutoReset<bool> beginning(&is_beginning_operation_, true);
  if (!operation_raw) {
    DidFinish(id, std::move(callback), error);
    return id;
  }
  PrepareForWrite(id, url);
  operation_raw->Remove(url, recursive,
                        base::BindOnce(&FileSystemOperationRunner::DidFinish,
                                       weak_ptr_, id, std::move(callback)));
  return id;
}

OperationID FileSystemOperationRunner::Write(
    const FileSystemURL& url,
    std::unique_ptr<BlobDataHandle> blob,
    int64_t offset,
    const WriteCallback& callback) {
  base::File::Error error = base::File::FILE_OK;
  std::unique_ptr<FileSystemOperation> operation =
      file_system_context_->CreateFileSystemOperation(OperationType::kWrite,
                                                      url, &error);
  FileSystemOperation* operation_raw = operation.get();
  OperationID id = BeginOperation(std::move(operation));
  base::AutoReset<bool> beginning(&is_beginning_operation_, true);
  if (!operation_raw) {
    DidWrite(id, callback, error, 0, true);
    return id;
  }

  std::unique_ptr<FileStreamWriter> writer(
      file_system_context_->CreateFileStreamWriter(url, offset));
  if (!writer) {
    // Write is not supported.
    DidWrite(id, callback, base::File::FILE_ERROR_SECURITY, 0, true);
    return id;
  }

  auto writer_delegate = std::make_unique<FileWriterDelegate>(
      std::move(writer), url.mount_option().flush_policy());

  std::unique_ptr<BlobReader> blob_reader;
  if (blob)
    blob_reader = blob->CreateReader();

  PrepareForWrite(id, url);
  operation_raw->WriteBlob(
      url, std::move(writer_delegate), std::move(blob_reader),
      base::BindRepeating(&FileSystemOperationRunner::DidWrite, weak_ptr_, id,
                          callback));
  return id;
}

OperationID FileSystemOperationRunner::WriteStream(
    const FileSystemURL& url,
    mojo::ScopedDataPipeConsumerHandle data_pipe,
    int64_t offset,
    const WriteCallback& callback) {
  base::File::Error error = base::File::FILE_OK;
  std::unique_ptr<FileSystemOperation> operation =
      file_system_context_->CreateFileSystemOperation(OperationType::kWrite,
                                                      url, &error);
  FileSystemOperation* operation_raw = operation.get();
  OperationID id = BeginOperation(std::move(operation));
  base::AutoReset<bool> beginning(&is_beginning_operation_, true);
  if (!operation_raw) {
    DidWrite(id, callback, error, 0, true);
    return id;
  }

  std::unique_ptr<FileStreamWriter> writer(
      file_system_context_->CreateFileStreamWriter(url, offset));
  if (!writer) {
    // Write is not supported.
    DidWrite(id, callback, base::File::FILE_ERROR_SECURITY, 0, true);
    return id;
  }

  auto writer_delegate = std::make_unique<FileWriterDelegate>(
      std::move(writer), url.mount_option().flush_policy());

  PrepareForWrite(id, url);
  operation_raw->Write(url, std::move(writer_delegate), std::move(data_pipe),
                       base::BindRepeating(&FileSystemOperationRunner::DidWrite,
                                           weak_ptr_, id, callback));
  return id;
}

OperationID FileSystemOperationRunner::Truncate(const FileSystemURL& url,
                                                int64_t length,
                                                StatusCallback callback) {
  base::File::Error error = base::File::FILE_OK;
  std::unique_ptr<FileSystemOperation> operation =
      file_system_context_->CreateFileSystemOperation(OperationType::kTruncate,
                                                      url, &error);
  FileSystemOperation* operation_raw = operation.get();
  OperationID id = BeginOperation(std::move(operation));
  base::AutoReset<bool> beginning(&is_beginning_operation_, true);
  if (!operation_raw) {
    DidFinish(id, std::move(callback), error);
    return id;
  }
  PrepareForWrite(id, url);
  operation_raw->Truncate(url, length,
                          base::BindOnce(&FileSystemOperationRunner::DidFinish,
                                         weak_ptr_, id, std::move(callback)));
  return id;
}

void FileSystemOperationRunner::Cancel(OperationID id,
                                       StatusCallback callback) {
  if (base::Contains(finished_operations_, id)) {
    DCHECK(!base::Contains(stray_cancel_callbacks_, id));
    stray_cancel_callbacks_[id] = std::move(callback);
    return;
  }

  auto found = operations_.find(id);
  if (found == operations_.end() || !found->second) {
    // There is no operation with |id|.
    std::move(callback).Run(base::File::FILE_ERROR_INVALID_OPERATION);
    return;
  }
  found->second->Cancel(std::move(callback));
}

OperationID FileSystemOperationRunner::TouchFile(
    const FileSystemURL& url,
    const base::Time& last_access_time,
    const base::Time& last_modified_time,
    StatusCallback callback) {
  base::File::Error error = base::File::FILE_OK;
  std::unique_ptr<FileSystemOperation> operation =
      file_system_context_->CreateFileSystemOperation(OperationType::kTouchFile,
                                                      url, &error);
  FileSystemOperation* operation_raw = operation.get();
  OperationID id = BeginOperation(std::move(operation));
  base::AutoReset<bool> beginning(&is_beginning_operation_, true);
  if (!operation_raw) {
    DidFinish(id, std::move(callback), error);
    return id;
  }
  PrepareForWrite(id, url);
  operation_raw->TouchFile(url, last_access_time, last_modified_time,
                           base::BindOnce(&FileSystemOperationRunner::DidFinish,
                                          weak_ptr_, id, std::move(callback)));
  return id;
}

OperationID FileSystemOperationRunner::OpenFile(const FileSystemURL& url,
                                                uint32_t file_flags,
                                                OpenFileCallback callback) {
  base::File::Error error = base::File::FILE_OK;
  std::unique_ptr<FileSystemOperation> operation =
      file_system_context_->CreateFileSystemOperation(OperationType::kOpenFile,
                                                      url, &error);
  FileSystemOperation* operation_raw = operation.get();
  OperationID id = BeginOperation(std::move(operation));
  base::AutoReset<bool> beginning(&is_beginning_operation_, true);
  if (!operation_raw) {
    DidOpenFile(id, std::move(callback), base::File(error),
                base::OnceClosure());
    return id;
  }

  // This file might be passed to an untrusted process.
  file_flags = base::File::AddFlagsForPassingToUntrustedProcess(file_flags);

  if (file_flags &
      (base::File::FLAG_CREATE | base::File::FLAG_OPEN_ALWAYS |
       base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_OPEN_TRUNCATED |
       base::File::FLAG_WRITE | base::File::FLAG_WIN_EXCLUSIVE_WRITE |
       base::File::FLAG_DELETE_ON_CLOSE | base::File::FLAG_WRITE_ATTRIBUTES)) {
    PrepareForWrite(id, url);
  } else {
    PrepareForRead(id, url);
  }
  operation_raw->OpenFile(
      url, file_flags,
      base::BindOnce(&FileSystemOperationRunner::DidOpenFile, weak_ptr_, id,
                     std::move(callback)));
  return id;
}

OperationID FileSystemOperationRunner::CreateSnapshotFile(
    const FileSystemURL& url,
    SnapshotFileCallback callback) {
  base::File::Error error = base::File::FILE_OK;
  std::unique_ptr<FileSystemOperation> operation =
      file_system_context_->CreateFileSystemOperation(
          OperationType::kCreateSnapshotFile, url, &error);
  FileSystemOperation* operation_raw = operation.get();
  OperationID id = BeginOperation(std::move(operation));
  base::AutoReset<bool> beginning(&is_beginning_operation_, true);
  if (!operation_raw) {
    DidCreateSnapshot(id, std::move(callback), error, base::File::Info(),
                      base::FilePath(), nullptr);
    return id;
  }
  PrepareForRead(id, url);
  operation_raw->CreateSnapshotFile(
      url, base::BindOnce(&FileSystemOperationRunner::DidCreateSnapshot,
                          weak_ptr_, id, std::move(callback)));
  return id;
}

OperationID FileSystemOperationRunner::CopyInForeignFile(
    const base::FilePath& src_local_disk_path,
    const FileSystemURL& dest_url,
    StatusCallback callback) {
  base::File::Error error = base::File::FILE_OK;
  std::unique_ptr<FileSystemOperation> operation =
      file_system_context_->CreateFileSystemOperation(
          OperationType::kCopyInForeignFile, dest_url, &error);
  FileSystemOperation* operation_raw = operation.get();
  OperationID id = BeginOperation(std::move(operation));
  base::AutoReset<bool> beginning(&is_beginning_operation_, true);
  if (!operation_raw) {
    DidFinish(id, std::move(callback), error);
    return id;
  }
  PrepareForWrite(id, dest_url);
  operation_raw->CopyInForeignFile(
      src_local_disk_path, dest_url,
      base::BindOnce(&FileSystemOperationRunner::DidFinish, weak_ptr_, id,
                     std::move(callback)));
  return id;
}

OperationID FileSystemOperationRunner::RemoveFile(const FileSystemURL& url,
                                                  StatusCallback callback) {
  base::File::Error error = base::File::FILE_OK;
  std::unique_ptr<FileSystemOperation> operation =
      file_system_context_->CreateFileSystemOperation(OperationType::kRemove,
                                                      url, &error);
  FileSystemOperation* operation_raw = operation.get();
  OperationID id = BeginOperation(std::move(operation));
  base::AutoReset<bool> beginning(&is_beginning_operation_, true);
  if (!operation_raw) {
    DidFinish(id, std::move(callback), error);
    return id;
  }
  PrepareForWrite(id, url);
  operation_raw->RemoveFile(
      url, base::BindOnce(&FileSystemOperationRunner::DidFinish, weak_ptr_, id,
                          std::move(callback)));
  return id;
}

OperationID FileSystemOperationRunner::RemoveDirectory(
    const FileSystemURL& url,
    StatusCallback callback) {
  base::File::Error error = base::File::FILE_OK;
  std::unique_ptr<FileSystemOperation> operation =
      file_system_context_->CreateFileSystemOperation(OperationType::kRemove,
                                                      url, &error);
  FileSystemOperation* operation_raw = operation.get();
  OperationID id = BeginOperation(std::move(operation));
  base::AutoReset<bool> beginning(&is_beginning_operation_, true);
  if (!operation_raw) {
    DidFinish(id, std::move(callback), error);
    return id;
  }
  PrepareForWrite(id, url);
  operation_raw->RemoveDirectory(
      url, base::BindOnce(&FileSystemOperationRunner::DidFinish, weak_ptr_, id,
                          std::move(callback)));
  return id;
}

OperationID FileSystemOperationRunner::CopyFileLocal(
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    CopyOrMoveOptionSet options,
    const CopyFileProgressCallback& progress_callback,
    StatusCallback callback) {
  base::File::Error error = base::File::FILE_OK;
  std::unique_ptr<FileSystemOperation> operation =
      file_system_context_->CreateFileSystemOperation(OperationType::kCopy,
                                                      src_url, &error);
  FileSystemOperation* operation_raw = operation.get();
  OperationID id = BeginOperation(std::move(operation));
  base::AutoReset<bool> beginning(&is_beginning_operation_, true);
  if (!operation_raw) {
    DidFinish(id, std::move(callback), error);
    return id;
  }
  PrepareForRead(id, src_url);
  PrepareForWrite(id, dest_url);
  operation_raw->CopyFileLocal(
      src_url, dest_url, options, progress_callback,
      base::BindOnce(&FileSystemOperationRunner::DidFinish, weak_ptr_, id,
                     std::move(callback)));
  return id;
}

OperationID FileSystemOperationRunner::MoveFileLocal(
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    CopyOrMoveOptionSet options,
    StatusCallback callback) {
  base::File::Error error = base::File::FILE_OK;
  std::unique_ptr<FileSystemOperation> operation =
      file_system_context_->CreateFileSystemOperation(OperationType::kMove,
                                                      src_url, &error);
  FileSystemOperation* operation_raw = operation.get();
  OperationID id = BeginOperation(std::move(operation));
  base::AutoReset<bool> beginning(&is_beginning_operation_, true);
  if (!operation_raw) {
    DidFinish(id, std::move(callback), error);
    return id;
  }
  PrepareForWrite(id, src_url);
  PrepareForWrite(id, dest_url);
  operation_raw->MoveFileLocal(
      src_url, dest_url, options,
      base::BindOnce(&FileSystemOperationRunner::DidFinish, weak_ptr_, id,
                     std::move(callback)));
  return id;
}

base::File::Error FileSystemOperationRunner::SyncGetPlatformPath(
    const FileSystemURL& url,
    base::FilePath* platform_path) {
  base::File::Error error = base::File::FILE_OK;
  std::unique_ptr<FileSystemOperation> operation(
      file_system_context_->CreateFileSystemOperation(
          OperationType::kGetLocalPath, url, &error));
  if (!operation.get())
    return error;
  return operation->SyncGetPlatformPath(url, platform_path);
}

FileSystemOperationRunner::FileSystemOperationRunner(
    FileSystemContext* file_system_context)
    : file_system_context_(file_system_context) {
  weak_ptr_ = weak_factory_.GetWeakPtr();
}

void FileSystemOperationRunner::DidFinish(const OperationID id,
                                          StatusCallback callback,
                                          base::File::Error rv) {
  // Calling the callback or deleting the |operations_| entry in
  // |FinishOperation| may release the FileSystemContext which owns this runner,
  // so take a reference to keep both alive until the end of this call.
  scoped_refptr<FileSystemContext> context(file_system_context_.get());

  if (is_beginning_operation_) {
    finished_operations_.insert(id);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&FileSystemOperationRunner::DidFinish,
                                  weak_ptr_, id, std::move(callback), rv));
    return;
  }
  std::move(callback).Run(rv);
  FinishOperation(id);
}

void FileSystemOperationRunner::DidGetMetadata(
    const OperationID id,
    GetMetadataCallback callback,
    base::File::Error rv,
    const base::File::Info& file_info) {
  // Calling the callback or deleting the |operations_| entry in
  // |FinishOperation| may release the FileSystemContext which owns this runner,
  // so take a reference to keep both alive until the end of this call.
  scoped_refptr<FileSystemContext> context(file_system_context_.get());

  if (is_beginning_operation_) {
    finished_operations_.insert(id);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&FileSystemOperationRunner::DidGetMetadata, weak_ptr_,
                       id, std::move(callback), rv, file_info));
    return;
  }
  std::move(callback).Run(rv, file_info);
  FinishOperation(id);
}

void FileSystemOperationRunner::DidReadDirectory(
    const OperationID id,
    const ReadDirectoryCallback& callback,
    base::File::Error rv,
    std::vector<filesystem::mojom::DirectoryEntry> entries,
    bool has_more) {
  // Calling the callback or deleting the |operations_| entry in
  // |FinishOperation| may release the FileSystemContext which owns this runner,
  // so take a reference to keep both alive until the end of this call.
  scoped_refptr<FileSystemContext> context(file_system_context_.get());

  if (is_beginning_operation_) {
    finished_operations_.insert(id);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&FileSystemOperationRunner::DidReadDirectory, weak_ptr_,
                       id, callback, rv, std::move(entries), has_more));
    return;
  }
  callback.Run(rv, std::move(entries), has_more);
  if (rv != base::File::FILE_OK || !has_more)
    FinishOperation(id);
}

void FileSystemOperationRunner::DidWrite(const OperationID id,
                                         const WriteCallback& callback,
                                         base::File::Error rv,
                                         int64_t bytes,
                                         bool complete) {
  // Calling the callback or deleting the |operations_| entry in
  // |FinishOperation| may release the FileSystemContext which owns this runner,
  // so take a reference to keep both alive until the end of this call.
  scoped_refptr<FileSystemContext> context(file_system_context_.get());

  if (is_beginning_operation_) {
    finished_operations_.insert(id);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&FileSystemOperationRunner::DidWrite, weak_ptr_, id,
                       callback, rv, bytes, complete));
    return;
  }
  callback.Run(rv, bytes, complete);
  if (rv != base::File::FILE_OK || complete)
    FinishOperation(id);
}

void FileSystemOperationRunner::DidOpenFile(
    const OperationID id,
    OpenFileCallback callback,
    base::File file,
    base::OnceClosure on_close_callback) {
  // Calling the callback or deleting the |operations_| entry in
  // |FinishOperation| may release the FileSystemContext which owns this runner,
  // so take a reference to keep both alive until the end of this call.
  scoped_refptr<FileSystemContext> context(file_system_context_.get());

  if (is_beginning_operation_) {
    finished_operations_.insert(id);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&FileSystemOperationRunner::DidOpenFile, weak_ptr_, id,
                       std::move(callback), std::move(file),
                       std::move(on_close_callback)));
    return;
  }
  base::ScopedClosureRunner scoped_on_close_callback;
  if (on_close_callback) {
    // Wrap `on_close_callback` to ensure it always runs, and on the IO thread.
    scoped_on_close_callback = base::ScopedClosureRunner(
        base::BindPostTaskToCurrentDefault(std::move(on_close_callback)));
  }

  std::move(callback).Run(std::move(file), std::move(scoped_on_close_callback));
  FinishOperation(id);
}

void FileSystemOperationRunner::DidCreateSnapshot(
    const OperationID id,
    SnapshotFileCallback callback,
    base::File::Error rv,
    const base::File::Info& file_info,
    const base::FilePath& platform_path,
    scoped_refptr<ShareableFileReference> file_ref) {
  // Calling the callback or deleting the |operations_| entry in
  // |FinishOperation| may release the FileSystemContext which owns this runner,
  // so take a reference to keep both alive until the end of this call.
  scoped_refptr<FileSystemContext> context(file_system_context_.get());

  if (is_beginning_operation_) {
    finished_operations_.insert(id);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&FileSystemOperationRunner::DidCreateSnapshot, weak_ptr_,
                       id, std::move(callback), rv, file_info, platform_path,
                       std::move(file_ref)));
    return;
  }
  std::move(callback).Run(rv, file_info, platform_path, std::move(file_ref));
  FinishOperation(id);
}

void FileSystemOperationRunner::PrepareForWrite(OperationID id,
                                                const FileSystemURL& url) {
  if (file_system_context_->GetUpdateObservers(url.type())) {
    file_system_context_->GetUpdateObservers(url.type())
        ->Notify(&FileUpdateObserver::OnStartUpdate, url);
  }
  write_target_urls_[id].insert(url);
}

void FileSystemOperationRunner::PrepareForRead(OperationID id,
                                               const FileSystemURL& url) {
  if (file_system_context_->GetAccessObservers(url.type())) {
    file_system_context_->GetAccessObservers(url.type())
        ->Notify(&FileAccessObserver::OnAccess, url);
  }
}

OperationID FileSystemOperationRunner::BeginOperation(
    std::unique_ptr<FileSystemOperation> operation) {
  OperationID id = next_operation_id_++;

  DCHECK(!base::Contains(operations_, id));
  operations_[id] = std::move(operation);
  return id;
}

void FileSystemOperationRunner::FinishOperation(OperationID id) {
  auto found = write_target_urls_.find(id);
  if (found != write_target_urls_.end()) {
    const FileSystemURLSet& urls = found->second;
    for (const FileSystemURL& url : urls) {
      if (file_system_context_->GetUpdateObservers(url.type())) {
        file_system_context_->GetUpdateObservers(url.type())
            ->Notify(&FileUpdateObserver::OnEndUpdate, url);
      }
    }
    write_target_urls_.erase(found);
  }

  operations_.erase(id);
  finished_operations_.erase(id);

  // Dispatch stray cancel callback if exists.
  auto found_cancel = stray_cancel_callbacks_.find(id);
  if (found_cancel != stray_cancel_callbacks_.end()) {
    // This cancel has been requested after the operation has finished,
    // so report that we failed to stop it.
    std::move(found_cancel->second)
        .Run(base::File::FILE_ERROR_INVALID_OPERATION);
    stray_cancel_callbacks_.erase(found_cancel);
  }
}

}  // namespace storage

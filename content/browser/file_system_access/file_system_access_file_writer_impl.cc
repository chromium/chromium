// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_file_writer_impl.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/file_system_access/file_system_access_error.h"
#include "content/browser/file_system_access/file_system_access_manager_impl.h"
#include "content/browser/file_system_access/file_system_access_safe_move_helper.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "third_party/blink/public/common/features_generated.h"

using blink::mojom::FileSystemAccessStatus;
using storage::FileSystemOperation;
using storage::FileSystemOperationRunner;

namespace content {

struct FileSystemAccessFileWriterImpl::WriteState {
  WriteCallback callback;
  uint64_t bytes_written = 0;
};

FileSystemAccessFileWriterImpl::FileSystemAccessFileWriterImpl(
    FileSystemAccessManagerImpl* manager,
    base::PassKey<FileSystemAccessManagerImpl> pass_key,
    const BindingContext& context,
    const storage::FileSystemURL& url,
    const storage::FileSystemURL& swap_url,
    scoped_refptr<FileSystemAccessLockManager::LockHandle> lock,
    scoped_refptr<FileSystemAccessLockManager::LockHandle> swap_lock,
    const SharedHandleState& handle_state,
    mojo::PendingReceiver<blink::mojom::FileSystemAccessFileWriter> receiver,
    bool has_transient_user_activation,
    bool auto_close,
    download::QuarantineConnectionCallback quarantine_connection_callback)
    : FileSystemAccessHandleBase(manager, context, url, handle_state),
      receiver_(this, std::move(receiver)),
      swap_url_(swap_url),
      lock_(std::move(lock)),
      swap_lock_(std::move(swap_lock)),
      quarantine_connection_callback_(
          std::move(quarantine_connection_callback)),
      has_transient_user_activation_(has_transient_user_activation),
      auto_close_(auto_close) {
  CHECK_EQ(swap_url.type(), url.type());
  CHECK(swap_lock_->IsExclusive());

  receiver_.set_disconnect_handler(base::BindOnce(
      &FileSystemAccessFileWriterImpl::OnDisconnect, base::Unretained(this)));
}

FileSystemAccessFileWriterImpl::~FileSystemAccessFileWriterImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (should_purge_swap_file_on_destruction_) {
    manager()->DoFileSystemOperation(
        FROM_HERE, &FileSystemOperationRunner::RemoveFile,
        base::BindOnce(
            [](const storage::FileSystemURL& swap_url,
               base::File::Error result) {
              if (result != base::File::FILE_OK &&
                  result != base::File::FILE_ERROR_NOT_FOUND) {
                DLOG(ERROR) << "Error Deleting Swap File, status: "
                            << base::File::ErrorToString(result)
                            << " path: " << swap_url.path();
              }
            },
            swap_url()),
        swap_url());
  }
}

void FileSystemAccessFileWriterImpl::Write(
    uint64_t offset,
    mojo::ScopedDataPipeConsumerHandle stream,
    WriteCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  RunWithWritePermission(
      base::BindOnce(&FileSystemAccessFileWriterImpl::WriteImpl,
                     weak_factory_.GetWeakPtr(), offset, std::move(stream)),
      base::BindOnce([](blink::mojom::FileSystemAccessErrorPtr result,
                        WriteCallback callback) {
        std::move(callback).Run(std::move(result),
                                /*bytes_written=*/0);
      }),
      std::move(callback));
}

void FileSystemAccessFileWriterImpl::Truncate(uint64_t length,
                                              TruncateCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  RunWithWritePermission(
      base::BindOnce(&FileSystemAccessFileWriterImpl::TruncateImpl,
                     weak_factory_.GetWeakPtr(), length),
      base::BindOnce([](blink::mojom::FileSystemAccessErrorPtr result,
                        TruncateCallback callback) {
        std::move(callback).Run(std::move(result));
      }),
      std::move(callback));
}

void FileSystemAccessFileWriterImpl::Close(CloseCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  RunWithWritePermission(
      base::BindOnce(&FileSystemAccessFileWriterImpl::CloseImpl,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce([](blink::mojom::FileSystemAccessErrorPtr result,
                        CloseCallback callback) {
        std::move(callback).Run(std::move(result));
      }),
      std::move(callback));
}

void FileSystemAccessFileWriterImpl::Abort(AbortCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  RunWithWritePermission(
      base::BindOnce(&FileSystemAccessFileWriterImpl::AbortImpl,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce([](blink::mojom::FileSystemAccessErrorPtr result,
                        AbortCallback callback) {
        std::move(callback).Run(std::move(result));
      }),
      std::move(callback));
}

// Do not call this method if `close_callback_` is not set.
void FileSystemAccessFileWriterImpl::CallCloseCallbackAndDeleteThis(
    blink::mojom::FileSystemAccessErrorPtr result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  should_purge_swap_file_on_destruction_ =
      result->status != blink::mojom::FileSystemAccessStatus::kOk;
  std::move(close_callback_).Run(std::move(result));

  // `this` is deleted after this call.
  manager()->RemoveFileWriter(this);
}

void FileSystemAccessFileWriterImpl::OnDisconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  receiver_.reset();

  if (is_close_pending()) {
    // Mojo connection lost while Close() in progress.
    return;
  }

  if (auto_close_) {
    // Close the Writer. `this` is deleted via
    // CallCloseCallbackAndDeleteThis when Close() finishes.
    Close(base::BindOnce([](blink::mojom::FileSystemAccessErrorPtr result) {
      if (result->status != blink::mojom::FileSystemAccessStatus::kOk) {
        DLOG(ERROR) << "AutoClose failed with result:"
                    << base::File::ErrorToString(result->file_error);
      }
    }));
    return;
  }

  // Mojo connection severed before Close() called. Destroy `this`.
  manager()->RemoveFileWriter(this);
}

void FileSystemAccessFileWriterImpl::WriteImpl(
    uint64_t offset,
    mojo::ScopedDataPipeConsumerHandle stream,
    WriteCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(GetWritePermissionStatus(),
            blink::mojom::PermissionStatus::GRANTED);

  if (is_close_pending()) {
    std::move(callback).Run(
        file_system_access_error::FromStatus(
            FileSystemAccessStatus::kInvalidState,
            "An attempt was made to write to a closing writer."),
        /*bytes_written=*/0);
    return;
  }

  manager()->DoFileSystemOperation(
      FROM_HERE, &FileSystemOperationRunner::WriteStream,
      base::BindRepeating(&FileSystemAccessFileWriterImpl::DidWrite,
                          weak_factory_.GetWeakPtr(),
                          base::Owned(new WriteState{std::move(callback)})),
      swap_url(), std::move(stream), offset);
}

void FileSystemAccessFileWriterImpl::DidWrite(WriteState* state,
                                              base::File::Error result,
                                              int64_t bytes,
                                              bool complete) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(state);
  state->bytes_written += bytes;
  if (complete) {
    std::move(state->callback)
        .Run(file_system_access_error::FromFileError(result),
             state->bytes_written);
  }
}

void FileSystemAccessFileWriterImpl::TruncateImpl(uint64_t length,
                                                  TruncateCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(GetWritePermissionStatus(),
            blink::mojom::PermissionStatus::GRANTED);

  if (is_close_pending()) {
    std::move(callback).Run(file_system_access_error::FromStatus(
        FileSystemAccessStatus::kInvalidState,
        "An attempt was made to write to a closing writer."));
    return;
  }

  manager()->DoFileSystemOperation(
      FROM_HERE, &FileSystemOperationRunner::Truncate,
      base::BindOnce(
          [](TruncateCallback callback, base::File::Error result) {
            std::move(callback).Run(
                file_system_access_error::FromFileError(result));
          },
          std::move(callback)),
      swap_url(), length);
}

void FileSystemAccessFileWriterImpl::CloseImpl(CloseCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(GetWritePermissionStatus(),
            blink::mojom::PermissionStatus::GRANTED);
  if (is_close_pending()) {
    std::move(callback).Run(file_system_access_error::FromStatus(
        FileSystemAccessStatus::kInvalidState,
        "An attempt was made to close an already closing writer."));
    return;
  }

  close_callback_ = std::move(callback);

  auto file_system_access_safe_move_helper =
      std::make_unique<FileSystemAccessSafeMoveHelper>(
          manager()->AsWeakPtr(), context(),
          /*source_url=*/swap_url(),
          /*dest_url=*/url(),
          FileSystemOperation::CopyOrMoveOptionSet(
              {FileSystemOperation::CopyOrMoveOption::
                   kPreserveDestinationPermissions}),
          std::move(quarantine_connection_callback_),
          has_transient_user_activation_);
  // Allows the unique pointer to be bound to the callback so the helper stays
  // alive until the operation completes.
  FileSystemAccessSafeMoveHelper* raw_helper =
      file_system_access_safe_move_helper.get();
  raw_helper->Start(
      base::BindOnce(&FileSystemAccessFileWriterImpl::DidReplaceSwapFile,
                     weak_factory_.GetWeakPtr(),
                     std::move(file_system_access_safe_move_helper)));
}

void FileSystemAccessFileWriterImpl::AbortImpl(AbortCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_close_pending()) {
    std::move(callback).Run(file_system_access_error::FromStatus(
        FileSystemAccessStatus::kInvalidState,
        "An attempt was made to abort an already closing writer."));
    return;
  }

  auto_close_ = false;

  std::move(callback).Run(file_system_access_error::Ok());

  // `this` is deleted after this call.
  manager()->RemoveFileWriter(this);
}

void FileSystemAccessFileWriterImpl::DidReplaceSwapFile(
    std::unique_ptr<FileSystemAccessSafeMoveHelper> /*move_helper*/,
    blink::mojom::FileSystemAccessErrorPtr result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (result->status != FileSystemAccessStatus::kOk) {
    DLOG(ERROR) << "Swap file move operation failed source: "
                << swap_url().path() << " dest: " << url().path()
                << " error: " << base::File::ErrorToString(result->file_error);
    CallCloseCallbackAndDeleteThis(std::move(result));
    return;
  }

  CallCloseCallbackAndDeleteThis(file_system_access_error::Ok());
}

base::WeakPtr<FileSystemAccessHandleBase>
FileSystemAccessFileWriterImpl::AsWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_factory_.GetWeakPtr();
}

}  // namespace content

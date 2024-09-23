// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/copy_or_move_operation_delegate.h"

#include <cstdint>
#include <memory>
#include <tuple>
#include <utility>

#include "base/auto_reset.h"
#include "base/check_is_test.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/file_access/file_access_copy_or_move_delegate_factory.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "storage/browser/blob/shareable_file_reference.h"
#include "storage/browser/file_system/copy_or_move_file_validator.h"
#include "storage/browser/file_system/copy_or_move_hook_delegate.h"
#include "storage/browser/file_system/copy_or_move_hook_delegate_composite.h"
#include "storage/browser/file_system/file_observers.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "storage/browser/file_system/file_stream_writer.h"
#include "storage/browser/file_system/file_system_backend.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/common/file_system/file_system_util.h"

namespace storage {

const int64_t kFlushIntervalInBytes = 10 << 20;  // 10MB.

namespace {

base::File::Error MaybeSuppressError(
    base::File::Error error,
    CopyOrMoveHookDelegate::ErrorAction error_action) {
  if (error_action == CopyOrMoveHookDelegate::ErrorAction::kSkip) {
    return base::File::FILE_OK;
  }
  return error;
}

CopyOrMoveHookDelegate::ErrorCallback CreateErrorSuppressCallback(
    CopyOrMoveHookDelegate::StatusCallback callback,
    base::File::Error error) {
  return base::BindOnce(
      [](CopyOrMoveHookDelegate::StatusCallback callback,
         base::File::Error error,
         CopyOrMoveHookDelegate::ErrorAction error_action) {
        std::move(callback).Run(MaybeSuppressError(error, error_action));
      },
      std::move(callback), error);
}

}  // namespace

class CopyOrMoveOperationDelegate::CopyOrMoveImpl {
 public:
  CopyOrMoveImpl(const CopyOrMoveImpl&) = delete;
  CopyOrMoveImpl& operator=(const CopyOrMoveImpl&) = delete;

  virtual ~CopyOrMoveImpl() = default;
  virtual void Run(CopyOrMoveOperationDelegate::StatusCallback callback) = 0;
  virtual void Cancel() = 0;

  // Force any file copy to result in an error. This affects copies or
  // cross-filesystem moves.
  void ForceCopyErrorForTest() { force_error_for_test_ = true; }

 protected:
  CopyOrMoveImpl(
      FileSystemOperationRunner* operation_runner,
      const CopyOrMoveOperationDelegate::OperationType operation_type,
      const FileSystemURL& src_url,
      const FileSystemURL& dest_url,
      const CopyOrMoveOperationDelegate::CopyOrMoveOptionSet options,
      CopyOrMoveOperationDelegate* operation_delegate)
      : operation_runner_(operation_runner),
        operation_type_(operation_type),
        src_url_(src_url),
        dest_url_(dest_url),
        options_(options),
        operation_delegate_(operation_delegate) {}

  // Callback for sending progress events with the current number of processed
  // bytes.
  void OnCopyOrMoveFileProgress(int64_t size) {
    // `operation_delegate_` owns `this` and the delayed callbacks will not run
    // after it's destroyed.
    operation_delegate_->RunCopyOrMoveHookDelegateCallbackLater(
        &CopyOrMoveHookDelegate::OnProgress, src_url_, dest_url_, size);
  }

  // Callback for sending progress events notifying the end of a copy, for a
  // copy operation or a cross-filesystem move.
  void DidEndCopy(CopyOrMoveOperationDelegate::StatusCallback callback,
                  base::File::Error error) {
    if (error == base::File::FILE_ERROR_NOT_A_FILE) {
      // The item appears to be a directory: don't trigger the delegate,
      // continue recursive operations right away.
      if (!callback.is_null()) {
        std::move(callback).Run(error);
      }
      return;
    }
    if (error != base::File::FILE_OK) {
      // `operation_delegate_` owns `this` and the delayed callbacks will not
      // run after it's destroyed.
      operation_delegate_->RunCopyOrMoveHookDelegateCallbackLater(
          &CopyOrMoveHookDelegate::OnError, src_url_, dest_url_, error,
          base::BindOnce(&CopyOrMoveImpl::DidEndCopyAfterError,
                         weak_factory_.GetWeakPtr(), std::move(callback),
                         error));
      return;
    }
    // `operation_delegate_` owns `this` and the delayed callbacks will not run
    // after it's destroyed.
    operation_delegate_->RunCopyOrMoveHookDelegateCallbackLater(
        &CopyOrMoveHookDelegate::OnEndCopy, src_url_, dest_url_);
    if (!callback.is_null()) {
      std::move(callback).Run(error);
    }
  }

  // Callback to continue (or not, in case of a non-skipped error) operation
  // after notifying about a possible error.
  void DidEndCopyAfterError(
      CopyOrMoveOperationDelegate::StatusCallback callback,
      base::File::Error error,
      CopyOrMoveHookDelegate::ErrorAction error_action) {
    if (options_.Has(FileSystemOperation::CopyOrMoveOption::
                         kRemovePartiallyCopiedFilesOnError) &&
        error != base::File::FILE_OK &&
        error != base::File::FILE_ERROR_NOT_A_FILE) {
      // On error, remove the destination file.
      operation_runner_->Remove(
          dest_url_, /*recursive=*/false,
          base::BindOnce(&CopyOrMoveImpl::DidRemoveDestOnError,
                         weak_factory_.GetWeakPtr(), error,
                         std::move(callback)));
      return;
    }

    // The callback should be called in case of copy or error. The callback is
    // null if the operation type is OPERATION_MOVE (implemented as copy +
    // delete) and no error occurred.
    if (!callback.is_null())
      std::move(callback).Run(MaybeSuppressError(error, error_action));
  }

  // Callback for sending progress events notifying the end of a move operation
  // in the case of a local (same-filesystem) move.
  void DidEndMove(CopyOrMoveOperationDelegate::StatusCallback callback,
                  base::File::Error error) {
    if (error == base::File::FILE_ERROR_NOT_A_FILE) {
      // The item appears to be a directory: don't trigger the delegate,
      // continue recursive operations right away.
      std::move(callback).Run(error);
      return;
    }
    if (error != base::File::FILE_OK) {
      // `operation_delegate_` owns `this` and the delayed callbacks will not
      // run after it's destroyed.
      operation_delegate_->RunCopyOrMoveHookDelegateCallbackLater(
          &CopyOrMoveHookDelegate::OnError, src_url_, dest_url_, error,
          CreateErrorSuppressCallback(std::move(callback), error));
      return;
    }
    // `operation_delegate_` owns `this` and the delayed callbacks will not run
    // after it's destroyed.
    operation_delegate_->RunCopyOrMoveHookDelegateCallbackLater(
        &CopyOrMoveHookDelegate::OnEndMove, src_url_, dest_url_);
    std::move(callback).Run(error);
  }

  // Callback for sending progress events notifying that the source entry has
  // been deleted in the case of a cross-filesystem move.
  void DidEndRemoveSourceForMove(
      CopyOrMoveOperationDelegate::StatusCallback callback,
      base::File::Error error) {
    if (error != base::File::FILE_OK) {
      // `operation_delegate_` owns `this` and the delayed callbacks will not
      // run after it's destroyed.
      operation_delegate_->RunCopyOrMoveHookDelegateCallbackLater(
          &CopyOrMoveHookDelegate::OnError, src_url_, dest_url_, error,
          CreateErrorSuppressCallback(std::move(callback), error));
      return;
    }
    // `operation_delegate_` owns `this` and the delayed callbacks will not run
    // after it's destroyed.
    operation_delegate_->RunCopyOrMoveHookDelegateCallbackLater(
        &CopyOrMoveHookDelegate::OnEndRemoveSource, src_url_);
    std::move(callback).Run(error);
  }

  void DidRemoveDestOnError(
      base::File::Error prior_error,
      CopyOrMoveOperationDelegate::StatusCallback callback,
      base::File::Error error) {
    if (error != base::File::FILE_OK) {
      VLOG(1) << "Error removing destination file after copy error or "
                 "cancellation: "
              << error;
    }
    // The callback is null if the operation type is OPERATION_MOVE (implemented
    // as copy + delete) and no error occurred.
    if (!callback.is_null()) {
      std::move(callback).Run(prior_error);
    }
  }

  const raw_ptr<FileSystemOperationRunner> operation_runner_;
  const CopyOrMoveOperationDelegate::OperationType operation_type_;
  const FileSystemURL src_url_;
  const FileSystemURL dest_url_;
  const CopyOrMoveOperationDelegate::CopyOrMoveOptionSet options_;
  bool force_error_for_test_ = false;

 private:
  raw_ptr<CopyOrMoveOperationDelegate> operation_delegate_;
  base::WeakPtrFactory<CopyOrMoveImpl> weak_factory_{this};
};

namespace {

// A non-owning pointer. Whoever calls `SetErrorUrlForTest` of
// `CopyOrMoveOperationDelegate` should take care of its lifespan.
const FileSystemURL* g_error_url_for_test = nullptr;

// Copies or moves a file on a (same) file system. Just delegate the operation
// to |operation_runner|.
class CopyOrMoveOnSameFileSystemImpl
    : public CopyOrMoveOperationDelegate::CopyOrMoveImpl {
 public:
  CopyOrMoveOnSameFileSystemImpl(
      FileSystemOperationRunner* operation_runner,
      const CopyOrMoveOperationDelegate::OperationType operation_type,
      const FileSystemURL& src_url,
      const FileSystemURL& dest_url,
      const CopyOrMoveOperationDelegate::CopyOrMoveOptionSet options,
      CopyOrMoveOperationDelegate* operation_delegate)
      : CopyOrMoveImpl(operation_runner,
                       operation_type,
                       src_url,
                       dest_url,
                       options,
                       operation_delegate) {}

  CopyOrMoveOnSameFileSystemImpl(const CopyOrMoveOnSameFileSystemImpl&) =
      delete;
  CopyOrMoveOnSameFileSystemImpl& operator=(
      const CopyOrMoveOnSameFileSystemImpl&) = delete;

  void Run(CopyOrMoveOperationDelegate::StatusCallback callback) override {
    if (operation_type_ == CopyOrMoveOperationDelegate::OPERATION_MOVE) {
      operation_runner_->MoveFileLocal(
          src_url_, dest_url_, options_,
          base::BindOnce(&CopyOrMoveOnSameFileSystemImpl::DidEndMove,
                         weak_factory_.GetWeakPtr(), std::move(callback)));
    } else {
      operation_runner_->CopyFileLocal(
          src_url_, dest_url_, options_,
          base::BindRepeating(
              &CopyOrMoveOnSameFileSystemImpl::OnCopyOrMoveFileProgress,
              weak_factory_.GetWeakPtr()),
          base::BindOnce(&CopyOrMoveOnSameFileSystemImpl::DidEndCopy,
                         weak_factory_.GetWeakPtr(), std::move(callback)));
    }
  }

  void Cancel() override {
    // We can do nothing for the copy/move operation on a local file system.
    // Assuming the operation is quickly done, it should be ok to just wait
    // for the completion.
  }

 private:
  base::WeakPtrFactory<CopyOrMoveOnSameFileSystemImpl> weak_factory_{this};
};

// Specifically for cross file system copy/move operation, this class creates
// a snapshot file, validates it if necessary, runs copying process,
// validates the created file, and removes source file for move (noop for
// copy).
class SnapshotCopyOrMoveImpl
    : public CopyOrMoveOperationDelegate::CopyOrMoveImpl {
 public:
  SnapshotCopyOrMoveImpl(
      FileSystemOperationRunner* operation_runner,
      CopyOrMoveOperationDelegate::OperationType operation_type,
      const FileSystemURL& src_url,
      const FileSystemURL& dest_url,
      CopyOrMoveOperationDelegate::CopyOrMoveOptionSet options,
      CopyOrMoveFileValidatorFactory* validator_factory,
      CopyOrMoveOperationDelegate* operation_delegate)
      : CopyOrMoveImpl(operation_runner,
                       operation_type,
                       src_url,
                       dest_url,
                       options,
                       operation_delegate),

        validator_factory_(validator_factory),
        cancel_requested_(false) {}

  SnapshotCopyOrMoveImpl(const SnapshotCopyOrMoveImpl&) = delete;
  SnapshotCopyOrMoveImpl& operator=(const SnapshotCopyOrMoveImpl&) = delete;

  void Run(CopyOrMoveOperationDelegate::StatusCallback callback) override {
    OnCopyOrMoveFileProgress(0);
    operation_runner_->CreateSnapshotFile(
        src_url_,
        base::BindOnce(&SnapshotCopyOrMoveImpl::RunAfterCreateSnapshot,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void Cancel() override { cancel_requested_ = true; }

 private:
  void RunAfterCreateSnapshot(
      CopyOrMoveOperationDelegate::StatusCallback callback,
      base::File::Error error,
      const base::File::Info& file_info,
      const base::FilePath& platform_path,
      scoped_refptr<ShareableFileReference> file_ref) {
    if (cancel_requested_)
      error = base::File::FILE_ERROR_ABORT;

    if (error != base::File::FILE_OK) {
      DidEndCopy(std::move(callback), error);
      return;
    }

    // For now we assume CreateSnapshotFile always return a valid local file
    // path.
    DCHECK(!platform_path.empty());

    if (!validator_factory_) {
      // No validation is needed.
      RunAfterPreWriteValidation(platform_path, file_info, std::move(file_ref),
                                 std::move(callback), base::File::FILE_OK);
      return;
    }

    // Run pre write validation.
    PreWriteValidation(
        platform_path,
        base::BindOnce(&SnapshotCopyOrMoveImpl::RunAfterPreWriteValidation,
                       weak_factory_.GetWeakPtr(), platform_path, file_info,
                       std::move(file_ref), std::move(callback)));
  }

  void RunAfterPreWriteValidation(
      const base::FilePath& platform_path,
      const base::File::Info& file_info,
      scoped_refptr<ShareableFileReference> file_ref,
      CopyOrMoveOperationDelegate::StatusCallback callback,
      base::File::Error error) {
    if (cancel_requested_)
      error = base::File::FILE_ERROR_ABORT;

    if (error != base::File::FILE_OK) {
      DidEndCopy(std::move(callback), error);
      return;
    }

    // |file_ref| is unused but necessary to keep the file alive until
    // CopyInForeignFile() is completed.
    operation_runner_->CopyInForeignFile(
        platform_path, dest_url_,
        base::BindOnce(&SnapshotCopyOrMoveImpl::RunAfterCopyInForeignFile,
                       weak_factory_.GetWeakPtr(), file_info,
                       std::move(file_ref), std::move(callback)));
  }

  void RunAfterCopyInForeignFile(
      const base::File::Info& file_info,
      scoped_refptr<ShareableFileReference> file_ref,
      CopyOrMoveOperationDelegate::StatusCallback callback,
      base::File::Error error) {
    if (cancel_requested_)
      error = base::File::FILE_ERROR_ABORT;

    if (error != base::File::FILE_OK) {
      DidEndCopy(std::move(callback), error);
      return;
    }

    OnCopyOrMoveFileProgress(file_info.size);

    if (options_.empty()) {
      RunAfterTouchFile(std::move(callback), base::File::FILE_OK);
      return;
    }

    operation_runner_->TouchFile(
        dest_url_, base::Time::Now() /* last_access */, file_info.last_modified,
        base::BindOnce(&SnapshotCopyOrMoveImpl::RunAfterTouchFile,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void RunAfterTouchFile(CopyOrMoveOperationDelegate::StatusCallback callback,
                         base::File::Error error) {
    if (cancel_requested_) {
      DidEndCopy(std::move(callback), base::File::FILE_ERROR_ABORT);
      return;
    }

    // |validator_| is nullptr when the destination filesystem does not do
    // validation.
    if (!validator_) {
      // No validation is needed. If TouchFile failed to restore the "last
      // modified" time, just ignore the error.
      RunAfterPostWriteValidation(std::move(callback), base::File::FILE_OK);
      return;
    }

    PostWriteValidation(
        base::BindOnce(&SnapshotCopyOrMoveImpl::RunAfterPostWriteValidation,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void RunAfterPostWriteValidation(
      CopyOrMoveOperationDelegate::StatusCallback callback,
      base::File::Error error) {
    if (force_error_for_test_) {
      error = base::File::FILE_ERROR_FAILED;
    }
    if (cancel_requested_) {
      DidEndCopy(std::move(callback), base::File::FILE_ERROR_ABORT);
      return;
    }

    if (error != base::File::FILE_OK) {
      // Failed to validate. Remove the destination file.
      operation_runner_->Remove(
          dest_url_, true /* recursive */,
          base::BindOnce(&SnapshotCopyOrMoveImpl::DidRemoveDestForError,
                         weak_factory_.GetWeakPtr(), error,
                         std::move(callback)));
      return;
    }

    if (operation_type_ == CopyOrMoveOperationDelegate::OPERATION_COPY) {
      DidEndCopy(std::move(callback), base::File::FILE_OK);
      return;
    } else {
      DidEndCopy(CopyOrMoveOperationDelegate::StatusCallback(),
                 base::File::FILE_OK);
    }

    DCHECK_EQ(CopyOrMoveOperationDelegate::OPERATION_MOVE, operation_type_);

    // Remove the source for finalizing move operation.
    operation_runner_->Remove(
        src_url_, true /* recursive */,
        base::BindOnce(&SnapshotCopyOrMoveImpl::RunAfterRemoveSourceForMove,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void RunAfterRemoveSourceForMove(
      CopyOrMoveOperationDelegate::StatusCallback callback,
      base::File::Error error) {
    if (cancel_requested_)
      error = base::File::FILE_ERROR_ABORT;

    if (error == base::File::FILE_ERROR_NOT_FOUND)
      error = base::File::FILE_OK;
    DidEndRemoveSourceForMove(std::move(callback), error);
  }

  void DidRemoveDestForError(
      base::File::Error prior_error,
      CopyOrMoveOperationDelegate::StatusCallback callback,
      base::File::Error error) {
    if (error != base::File::FILE_OK) {
      VLOG(1) << "Error removing destination file after validation error: "
              << error;
    }
    DidEndCopy(std::move(callback), prior_error);
  }

  // Runs pre-write validation.
  void PreWriteValidation(
      const base::FilePath& platform_path,
      CopyOrMoveOperationDelegate::StatusCallback callback) {
    DCHECK(validator_factory_);
    validator_.reset(validator_factory_->CreateCopyOrMoveFileValidator(
        src_url_, platform_path));
    validator_->StartPreWriteValidation(std::move(callback));
  }

  // Runs post-write validation.
  void PostWriteValidation(
      CopyOrMoveOperationDelegate::StatusCallback callback) {
    operation_runner_->CreateSnapshotFile(
        dest_url_,
        base::BindOnce(
            &SnapshotCopyOrMoveImpl::PostWriteValidationAfterCreateSnapshotFile,
            weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void PostWriteValidationAfterCreateSnapshotFile(
      CopyOrMoveOperationDelegate::StatusCallback callback,
      base::File::Error error,
      const base::File::Info& file_info,
      const base::FilePath& platform_path,
      scoped_refptr<ShareableFileReference> file_ref) {
    if (cancel_requested_)
      error = base::File::FILE_ERROR_ABORT;

    if (error != base::File::FILE_OK) {
      std::move(callback).Run(error);
      return;
    }

    DCHECK(validator_);
    // Note: file_ref passed here to keep the file alive until after
    // the StartPostWriteValidation operation finishes.
    validator_->StartPostWriteValidation(
        platform_path,
        base::BindOnce(&SnapshotCopyOrMoveImpl::DidPostWriteValidation,
                       weak_factory_.GetWeakPtr(), std::move(file_ref),
                       std::move(callback)));
  }

  // |file_ref| is unused; it is passed here to make sure the reference is
  // alive until after post-write validation is complete.
  void DidPostWriteValidation(
      scoped_refptr<ShareableFileReference> file_ref,
      CopyOrMoveOperationDelegate::StatusCallback callback,
      base::File::Error error) {
    std::move(callback).Run(error);
  }

  raw_ptr<CopyOrMoveFileValidatorFactory> validator_factory_;
  std::unique_ptr<CopyOrMoveFileValidator> validator_;
  bool cancel_requested_;
  base::WeakPtrFactory<SnapshotCopyOrMoveImpl> weak_factory_{this};
};

// The size of buffer for StreamCopyHelper.
const int kReadBufferSize = 32768;

// To avoid too many progress callbacks, it should be called less
// frequently than 50ms.
const int kMinProgressCallbackInvocationSpanInMilliseconds = 50;

// Specifically for cross file system copy/move operation, this class uses
// stream reader and writer for copying. Validator is not supported, so if
// necessary SnapshotCopyOrMoveImpl should be used.
class StreamCopyOrMoveImpl
    : public CopyOrMoveOperationDelegate::CopyOrMoveImpl {
 public:
  StreamCopyOrMoveImpl(
      FileSystemOperationRunner* operation_runner,
      FileSystemContext* file_system_context,
      CopyOrMoveOperationDelegate::OperationType operation_type,
      const FileSystemURL& src_url,
      const FileSystemURL& dest_url,
      CopyOrMoveOperationDelegate::CopyOrMoveOptionSet options,
      std::unique_ptr<FileStreamReader> reader,
      std::unique_ptr<FileStreamWriter> writer,
      CopyOrMoveOperationDelegate* operation_delegate)
      : CopyOrMoveImpl(operation_runner,
                       operation_type,
                       src_url,
                       dest_url,
                       options,
                       operation_delegate),
        file_system_context_(file_system_context),
        reader_(std::move(reader)),
        writer_(std::move(writer)),
        cancel_requested_(false) {}

  StreamCopyOrMoveImpl(const StreamCopyOrMoveImpl&) = delete;
  StreamCopyOrMoveImpl& operator=(const StreamCopyOrMoveImpl&) = delete;

  void Run(CopyOrMoveOperationDelegate::StatusCallback callback) override {
    // Reader can be created even if the entry does not exist or the entry is
    // a directory. To check errors before destination file creation,
    // check metadata first.
    operation_runner_->GetMetadata(
        src_url_,
        {storage::FileSystemOperation::GetMetadataField::kIsDirectory,
         storage::FileSystemOperation::GetMetadataField::kLastModified},
        base::BindOnce(&StreamCopyOrMoveImpl::RunAfterGetMetadataForSource,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void Cancel() override {
    cancel_requested_ = true;
    if (copy_helper_)
      copy_helper_->Cancel();
  }

 private:
  void NotifyOnStartUpdate(const FileSystemURL& url) {
    if (file_system_context_->GetUpdateObservers(url.type())) {
      file_system_context_->GetUpdateObservers(url.type())
          ->Notify(&FileUpdateObserver::OnStartUpdate, url);
    }
  }

  void NotifyOnModifyFile(const FileSystemURL& url) {
    if (file_system_context_->GetChangeObservers(url.type())) {
      file_system_context_->GetChangeObservers(url.type())
          ->Notify(&FileChangeObserver::OnModifyFile, url);
    }
  }

  void NotifyOnEndUpdate(const FileSystemURL& url) {
    if (file_system_context_->GetUpdateObservers(url.type())) {
      file_system_context_->GetUpdateObservers(url.type())
          ->Notify(&FileUpdateObserver::OnEndUpdate, url);
    }
  }

  void RunAfterGetMetadataForSource(
      CopyOrMoveOperationDelegate::StatusCallback callback,
      base::File::Error error,
      const base::File::Info& file_info) {
    if (cancel_requested_)
      error = base::File::FILE_ERROR_ABORT;

    if (error != base::File::FILE_OK) {
      DidEndCopy(std::move(callback), error);
      return;
    }

    if (file_info.is_directory) {
      // If not a directory, failed with appropriate error code.
      std::move(callback).Run(base::File::FILE_ERROR_NOT_A_FILE);
      return;
    }

    // To use FileStreamWriter, we need to ensure the destination file exists.
    operation_runner_->CreateFile(
        dest_url_, true /* exclusive */,
        base::BindOnce(&StreamCopyOrMoveImpl::RunAfterCreateFileForDestination,
                       weak_factory_.GetWeakPtr(), std::move(callback),
                       file_info.last_modified));
  }

  void RunAfterCreateFileForDestination(
      CopyOrMoveOperationDelegate::StatusCallback callback,
      const base::Time& last_modified,
      base::File::Error error) {
    if (cancel_requested_)
      error = base::File::FILE_ERROR_ABORT;
    // This conversion is to return the consistent status code with
    // FileSystemFileUtil::Copy.
    if (error == base::File::FILE_ERROR_NOT_A_FILE)
      error = base::File::FILE_ERROR_INVALID_OPERATION;

    if (error != base::File::FILE_OK &&
        error != base::File::FILE_ERROR_EXISTS) {
      DidEndCopy(std::move(callback), error);
      return;
    }

    if (error == base::File::FILE_ERROR_EXISTS) {
      operation_runner_->Truncate(
          dest_url_, 0 /* length */,
          base::BindOnce(&StreamCopyOrMoveImpl::RunAfterTruncateForDestination,
                         weak_factory_.GetWeakPtr(), std::move(callback),
                         last_modified));
      return;
    }
    RunAfterTruncateForDestination(std::move(callback), last_modified,
                                   base::File::FILE_OK);
  }

  void RunAfterTruncateForDestination(
      CopyOrMoveOperationDelegate::StatusCallback callback,
      const base::Time& last_modified,
      base::File::Error error) {
    if (cancel_requested_)
      error = base::File::FILE_ERROR_ABORT;

    if (error != base::File::FILE_OK) {
      DidEndCopy(std::move(callback), error);
      return;
    }

    NotifyOnStartUpdate(dest_url_);
    DCHECK(!copy_helper_);
    copy_helper_ =
        std::make_unique<CopyOrMoveOperationDelegate::StreamCopyHelper>(
            std::move(reader_), std::move(writer_),
            dest_url_.mount_option().flush_policy(), kReadBufferSize,
            base::BindRepeating(&StreamCopyOrMoveImpl::OnCopyOrMoveFileProgress,
                                weak_factory_.GetWeakPtr()),
            base::Milliseconds(
                kMinProgressCallbackInvocationSpanInMilliseconds));
    copy_helper_->Run(base::BindOnce(&StreamCopyOrMoveImpl::RunAfterStreamCopy,
                                     weak_factory_.GetWeakPtr(),
                                     std::move(callback), last_modified));
  }

  void RunAfterStreamCopy(CopyOrMoveOperationDelegate::StatusCallback callback,
                          const base::Time& last_modified,
                          base::File::Error error) {
    // Destruct StreamCopyHelper to close the destination file.
    // This is important because some file system implementations update
    // timestamps on file close and thus it should happen before we call
    // TouchFile().
    copy_helper_.reset();

    NotifyOnModifyFile(dest_url_);
    NotifyOnEndUpdate(dest_url_);
    if (force_error_for_test_) {
      error = base::File::FILE_ERROR_FAILED;
    }
    if (cancel_requested_)
      error = base::File::FILE_ERROR_ABORT;

    if (error != base::File::FILE_OK) {
      DidEndCopy(std::move(callback), error);
      return;
    }

    if (!options_.Has(
            FileSystemOperation::CopyOrMoveOption::kPreserveLastModified)) {
      RunAfterTouchFile(std::move(callback), base::File::FILE_OK);
      return;
    }

    operation_runner_->TouchFile(
        dest_url_, base::Time::Now() /* last_access */, last_modified,
        base::BindOnce(&StreamCopyOrMoveImpl::RunAfterTouchFile,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void RunAfterTouchFile(CopyOrMoveOperationDelegate::StatusCallback callback,
                         base::File::Error error) {
    if (cancel_requested_) {
      DidEndCopy(std::move(callback), base::File::FILE_ERROR_ABORT);
      return;
    }

    // If TouchFile failed to restore the "last modified" time, just ignore the
    // error.
    if (operation_type_ == CopyOrMoveOperationDelegate::OPERATION_COPY) {
      DidEndCopy(std::move(callback), base::File::FILE_OK);
      return;
    }

    DidEndCopy(CopyOrMoveOperationDelegate::StatusCallback(),
               base::File::FILE_OK);

    DCHECK_EQ(CopyOrMoveOperationDelegate::OPERATION_MOVE, operation_type_);

    // Remove the source for finalizing move operation.
    operation_runner_->Remove(
        src_url_, false /* recursive */,
        base::BindOnce(&StreamCopyOrMoveImpl::RunAfterRemoveForMove,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void RunAfterRemoveForMove(
      CopyOrMoveOperationDelegate::StatusCallback callback,
      base::File::Error error) {
    if (cancel_requested_)
      error = base::File::FILE_ERROR_ABORT;
    if (error == base::File::FILE_ERROR_NOT_FOUND)
      error = base::File::FILE_OK;
    DidEndRemoveSourceForMove(std::move(callback), error);
  }

  scoped_refptr<FileSystemContext> file_system_context_;
  std::unique_ptr<FileStreamReader> reader_;
  std::unique_ptr<FileStreamWriter> writer_;
  std::unique_ptr<CopyOrMoveOperationDelegate::StreamCopyHelper> copy_helper_;
  bool cancel_requested_;
  base::WeakPtrFactory<StreamCopyOrMoveImpl> weak_factory_{this};
};

}  // namespace

CopyOrMoveOperationDelegate::StreamCopyHelper::StreamCopyHelper(
    std::unique_ptr<FileStreamReader> reader,
    std::unique_ptr<FileStreamWriter> writer,
    FlushPolicy flush_policy,
    int buffer_size,
    FileSystemOperation::CopyFileProgressCallback file_progress_callback,
    const base::TimeDelta& min_progress_callback_invocation_span)
    : reader_(std::move(reader)),
      writer_(std::move(writer)),
      flush_policy_(flush_policy),
      file_progress_callback_(std::move(file_progress_callback)),
      io_buffer_(base::MakeRefCounted<net::IOBufferWithSize>(buffer_size)),
      num_copied_bytes_(0),
      previous_flush_offset_(0),
      min_progress_callback_invocation_span_(
          min_progress_callback_invocation_span),
      cancel_requested_(false) {}

CopyOrMoveOperationDelegate::StreamCopyHelper::~StreamCopyHelper() = default;

void CopyOrMoveOperationDelegate::StreamCopyHelper::Run(
    StatusCallback callback) {
  DCHECK(callback);
  DCHECK(!completion_callback_);

  completion_callback_ = std::move(callback);

  file_progress_callback_.Run(0);
  last_progress_callback_invocation_time_ = base::Time::Now();
  Read();
}

void CopyOrMoveOperationDelegate::StreamCopyHelper::Cancel() {
  cancel_requested_ = true;
}

void CopyOrMoveOperationDelegate::StreamCopyHelper::Read() {
  int result = reader_->Read(
      io_buffer_.get(), io_buffer_->size(),
      base::BindOnce(&StreamCopyHelper::DidRead, weak_factory_.GetWeakPtr()));
  if (result != net::ERR_IO_PENDING)
    DidRead(result);
}

void CopyOrMoveOperationDelegate::StreamCopyHelper::DidRead(int result) {
  if (cancel_requested_) {
    std::move(completion_callback_).Run(base::File::FILE_ERROR_ABORT);
    return;
  }

  if (result < 0) {
    std::move(completion_callback_).Run(NetErrorToFileError(result));
    return;
  }

  if (result == 0) {
    // Here is the EOF.
    if (flush_policy_ == FlushPolicy::FLUSH_ON_COMPLETION)
      Flush(FlushMode::kEndOfFile);
    else
      std::move(completion_callback_).Run(base::File::FILE_OK);
    return;
  }

  Write(base::MakeRefCounted<net::DrainableIOBuffer>(io_buffer_, result));
}

void CopyOrMoveOperationDelegate::StreamCopyHelper::Write(
    scoped_refptr<net::DrainableIOBuffer> buffer) {
  DCHECK_GT(buffer->BytesRemaining(), 0);

  int result =
      writer_->Write(buffer.get(), buffer->BytesRemaining(),
                     base::BindOnce(&StreamCopyHelper::DidWrite,
                                    weak_factory_.GetWeakPtr(), buffer));
  if (result != net::ERR_IO_PENDING)
    DidWrite(buffer, result);
}

void CopyOrMoveOperationDelegate::StreamCopyHelper::DidWrite(
    scoped_refptr<net::DrainableIOBuffer> buffer,
    int result) {
  if (cancel_requested_) {
    std::move(completion_callback_).Run(base::File::FILE_ERROR_ABORT);
    return;
  }

  if (result < 0) {
    std::move(completion_callback_).Run(NetErrorToFileError(result));
    return;
  }

  buffer->DidConsume(result);
  num_copied_bytes_ += result;

  // Check the elapsed time since last |file_progress_callback_| invocation.
  // Make sure to report the last progress update (when there are no bytes
  // remaining) regardless of the time so consumers don't miss it.
  base::Time now = base::Time::Now();
  if (now - last_progress_callback_invocation_time_ >=
          min_progress_callback_invocation_span_ ||
      buffer->BytesRemaining() <= 0) {
    file_progress_callback_.Run(num_copied_bytes_);
    last_progress_callback_invocation_time_ = now;
  }

  if (buffer->BytesRemaining() > 0) {
    Write(buffer);
    return;
  }

  if (flush_policy_ == FlushPolicy::FLUSH_ON_COMPLETION &&
      (num_copied_bytes_ - previous_flush_offset_) > kFlushIntervalInBytes) {
    Flush(FlushMode::kDefault);
  } else {
    Read();
  }
}

void CopyOrMoveOperationDelegate::StreamCopyHelper::Flush(
    FlushMode flush_mode) {
  int result = writer_->Flush(
      flush_mode, base::BindOnce(&StreamCopyHelper::DidFlush,
                                 weak_factory_.GetWeakPtr(), flush_mode));
  if (result != net::ERR_IO_PENDING)
    DidFlush(flush_mode, result);
}

void CopyOrMoveOperationDelegate::StreamCopyHelper::DidFlush(
    FlushMode flush_mode,
    int result) {
  if (cancel_requested_) {
    std::move(completion_callback_).Run(base::File::FILE_ERROR_ABORT);
    return;
  }

  previous_flush_offset_ = num_copied_bytes_;
  switch (flush_mode) {
    case FlushMode::kEndOfFile:
      std::move(completion_callback_).Run(NetErrorToFileError(result));
      break;
    case FlushMode::kDefault:
      Read();
      break;
  }
}

CopyOrMoveOperationDelegate::CopyOrMoveOperationDelegate(
    FileSystemContext* file_system_context,
    const FileSystemURL& src_root,
    const FileSystemURL& dest_root,
    OperationType operation_type,
    CopyOrMoveOptionSet options,
    ErrorBehavior error_behavior,
    std::unique_ptr<CopyOrMoveHookDelegate> copy_or_move_hook_delegate,
    StatusCallback callback)
    : RecursiveOperationDelegate(file_system_context),
      src_root_(src_root),
      dest_root_(dest_root),
      operation_type_(operation_type),
      options_(options),
      error_behavior_(error_behavior),
      copy_or_move_hook_delegate_(std::move(copy_or_move_hook_delegate)),
      callback_(std::move(callback)) {
  DCHECK(copy_or_move_hook_delegate_);
  if (file_access::FileAccessCopyOrMoveDelegateFactory::HasInstance()) {
    copy_or_move_hook_delegate_ = CopyOrMoveHookDelegateComposite::CreateOrAdd(
        std::move(copy_or_move_hook_delegate_),
        file_access::FileAccessCopyOrMoveDelegateFactory::Get()->MakeHook());
  }

  // Force same_file_system_ = false if options include kForceCrossFilesystem.
  same_file_system_ =
      !options.Has(
          FileSystemOperation::CopyOrMoveOption::kForceCrossFilesystem) &&
      src_root_.IsInSameFileSystem(dest_root_);
}

CopyOrMoveOperationDelegate::~CopyOrMoveOperationDelegate() = default;

void CopyOrMoveOperationDelegate::Run() {
  // Not supported; this should never be called.
  NOTREACHED();
}

void CopyOrMoveOperationDelegate::RunRecursively() {
#if DCHECK_IS_ON()
  DCHECK(!did_run_);
  did_run_ = true;
#endif
  // Perform light-weight checks first.

  // It is an error to try to copy/move an entry into its child.
  if (same_file_system_ && src_root_.path().IsParent(dest_root_.path())) {
    std::move(callback_).Run(base::File::FILE_ERROR_INVALID_OPERATION);
    return;
  }

  if (same_file_system_ && src_root_.path() == dest_root_.path()) {
    // In JS API this should return error, but we return success because Pepper
    // wants to return success and we have a code path that returns error in
    // Blink for JS (http://crbug.com/329517).
    std::move(callback_).Run(base::File::FILE_OK);
    return;
  }

  // Start to process the source directory recursively.
  // TODO(kinuko): This could be too expensive for same_file_system_==true
  // and operation==MOVE case, probably we can just rename the root directory.
  // http://crbug.com/172187
  StartRecursiveOperation(
      src_root_, error_behavior_,
      base::BindOnce(&CopyOrMoveOperationDelegate::FinishOperation,
                     weak_factory_.GetWeakPtr()));
}

void CopyOrMoveOperationDelegate::FinishOperation(base::File::Error error) {
  // We post the callback as a task to ensure that other posted tasks are
  // completed before finishing the operation.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback_), error));
}

void CopyOrMoveOperationDelegate::ProcessFile(const FileSystemURL& src_url,
                                              StatusCallback callback) {
  const FileSystemURL dest_url = CreateDestURL(src_url);

  RunCopyOrMoveHookDelegateCallbackLater(
      &CopyOrMoveHookDelegate::OnBeginProcessFile, src_url, dest_url,
      base::BindOnce(&CopyOrMoveOperationDelegate::DoProcessFile,
                     weak_factory_.GetWeakPtr(), src_url, dest_url,
                     std::move(callback)));
}

void CopyOrMoveOperationDelegate::DoProcessFile(const FileSystemURL& src_url,
                                                FileSystemURL dest_url,
                                                StatusCallback callback,
                                                base::File::Error error) {
  if (error != base::File::FILE_OK) {
    RunCopyOrMoveHookDelegateCallbackLater(
        &CopyOrMoveHookDelegate::OnError, src_url, dest_url, error,
        CreateErrorSuppressCallback(std::move(callback), error));
    return;
  }

  std::unique_ptr<CopyOrMoveImpl> impl;
  if (same_file_system_ &&
      (file_system_context()
           ->GetFileSystemBackend(src_url.type())
           ->HasInplaceCopyImplementation(src_url.type()) ||
       operation_type_ == OPERATION_MOVE)) {
    impl = std::make_unique<CopyOrMoveOnSameFileSystemImpl>(
        operation_runner(), operation_type_, src_url, dest_url, options_, this);
  } else {
    // Cross filesystem case.
    base::File::Error get_validator_factory_error =
        base::File::FILE_ERROR_FAILED;
    CopyOrMoveFileValidatorFactory* validator_factory =
        file_system_context()->GetCopyOrMoveFileValidatorFactory(
            dest_root_.type(), &get_validator_factory_error);
    if (get_validator_factory_error != base::File::FILE_OK) {
      RunCopyOrMoveHookDelegateCallbackLater(
          &CopyOrMoveHookDelegate::OnError, src_url, dest_url,
          get_validator_factory_error,
          CreateErrorSuppressCallback(std::move(callback),
                                      get_validator_factory_error));
      return;
    }

    if (!validator_factory) {
      std::unique_ptr<FileStreamReader> reader =
          file_system_context()->CreateFileStreamReader(
              src_url, 0 /* offset */, kMaximumLength, base::Time());
      std::unique_ptr<FileStreamWriter> writer =
          file_system_context()->CreateFileStreamWriter(dest_url, 0);
      if (reader && writer) {
        impl = std::make_unique<StreamCopyOrMoveImpl>(
            operation_runner(), file_system_context(), operation_type_, src_url,
            dest_url, options_, std::move(reader), std::move(writer), this);
      }
    }

    if (!impl) {
      impl = std::make_unique<SnapshotCopyOrMoveImpl>(
          operation_runner(), operation_type_, src_url, dest_url, options_,
          validator_factory, this);
    }
  }

  // Register the running task.
  CopyOrMoveImpl* impl_ptr = impl.get();
  running_copy_set_[impl_ptr] = std::move(impl);
  if (g_error_url_for_test && src_url == *g_error_url_for_test) {
    CHECK_IS_TEST();
    impl_ptr->ForceCopyErrorForTest();  // IN-TEST
  }
  impl_ptr->Run(base::BindOnce(&CopyOrMoveOperationDelegate::DidCopyOrMoveFile,
                               weak_factory_.GetWeakPtr(), std::move(callback),
                               impl_ptr));
}

void CopyOrMoveOperationDelegate::ProcessDirectory(const FileSystemURL& src_url,
                                                   StatusCallback callback) {
  if (src_url == src_root_) {
    // The src_root_ looks to be a directory.
    // Try removing the dest_root_ to see if it exists and/or it is an
    // empty directory.
    // We do not invoke |progress_callback_| for source root, because it is
    // already called in ProcessFile().
    operation_runner()->RemoveDirectory(
        dest_root_,
        base::BindOnce(&CopyOrMoveOperationDelegate::DidTryRemoveDestRoot,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }

  const FileSystemURL dest_url = CreateDestURL(src_url);

  RunCopyOrMoveHookDelegateCallbackLater(
      &CopyOrMoveHookDelegate::OnBeginProcessDirectory, src_url, dest_url,
      base::BindOnce(&CopyOrMoveOperationDelegate::ProcessDirectoryInternal,
                     weak_factory_.GetWeakPtr(), src_url, dest_url,
                     std::move(callback)));
}

void CopyOrMoveOperationDelegate::PostProcessDirectory(
    const FileSystemURL& src_url,
    StatusCallback callback) {
  if (options_.empty()) {
    PostProcessDirectoryAfterTouchFile(src_url, std::move(callback),
                                       base::File::FILE_OK);
    return;
  }

  operation_runner()->GetMetadata(
      src_url, {storage::FileSystemOperation::GetMetadataField::kLastModified},
      base::BindOnce(
          &CopyOrMoveOperationDelegate::PostProcessDirectoryAfterGetMetadata,
          weak_factory_.GetWeakPtr(), src_url, std::move(callback)));
}

base::WeakPtr<RecursiveOperationDelegate>
CopyOrMoveOperationDelegate::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

// static
void CopyOrMoveOperationDelegate::SetErrorUrlForTest(const FileSystemURL* url) {
  g_error_url_for_test = url;
}

void CopyOrMoveOperationDelegate::OnCancel() {
  // Request to cancel all running Copy/Move file.
  for (auto& job : running_copy_set_)
    job.first->Cancel();
}

void CopyOrMoveOperationDelegate::DidCopyOrMoveFile(StatusCallback callback,
                                                    CopyOrMoveImpl* impl,
                                                    base::File::Error error) {
  running_copy_set_.erase(impl);

  std::move(callback).Run(error);
}

void CopyOrMoveOperationDelegate::DidTryRemoveDestRoot(
    StatusCallback callback,
    base::File::Error error) {
  if (error == base::File::FILE_ERROR_NOT_A_DIRECTORY) {
    std::move(callback).Run(base::File::FILE_ERROR_INVALID_OPERATION);
    return;
  }
  if (error != base::File::FILE_OK &&
      error != base::File::FILE_ERROR_NOT_FOUND) {
    std::move(callback).Run(error);
    return;
  }

  ProcessDirectoryInternal(src_root_, dest_root_, std::move(callback),
                           base::File::FILE_OK);
}

void CopyOrMoveOperationDelegate::ProcessDirectoryInternal(
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    StatusCallback callback,
    base::File::Error error) {
  if (error != base::File::FILE_OK) {
    RunCopyOrMoveHookDelegateCallbackLater(
        &CopyOrMoveHookDelegate::OnError, src_url, dest_url, error,
        CreateErrorSuppressCallback(std::move(callback), error));
    return;
  }

  // If operation_type == Move we may need to record directories and
  // restore directory timestamps in the end, though it may have
  // negative performance impact.
  // See http://crbug.com/171284 for more details.
  operation_runner()->CreateDirectory(
      dest_url, false /* exclusive */, false /* recursive */,
      base::BindOnce(&CopyOrMoveOperationDelegate::DidCreateDirectory,
                     weak_factory_.GetWeakPtr(), src_url, dest_url,
                     std::move(callback)));
}

void CopyOrMoveOperationDelegate::DidCreateDirectory(
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    StatusCallback callback,
    base::File::Error error) {
  if (error != base::File::FILE_OK) {
    RunCopyOrMoveHookDelegateCallbackLater(
        &CopyOrMoveHookDelegate::OnError, src_url, dest_url, error,
        CreateErrorSuppressCallback(std::move(callback), error));
    return;
  }
  RunCopyOrMoveHookDelegateCallbackLater(&CopyOrMoveHookDelegate::OnEndCopy,
                                         src_url, dest_url);
  std::move(callback).Run(error);
}

void CopyOrMoveOperationDelegate::PostProcessDirectoryAfterGetMetadata(
    const FileSystemURL& src_url,
    StatusCallback callback,
    base::File::Error error,
    const base::File::Info& file_info) {
  if (error != base::File::FILE_OK) {
    // Ignore the error, and run post process which should run after TouchFile.
    PostProcessDirectoryAfterTouchFile(src_url, std::move(callback),
                                       base::File::FILE_OK);
    return;
  }

  operation_runner()->TouchFile(
      CreateDestURL(src_url), base::Time::Now() /* last access */,
      file_info.last_modified,
      base::BindOnce(
          &CopyOrMoveOperationDelegate::PostProcessDirectoryAfterTouchFile,
          weak_factory_.GetWeakPtr(), src_url, std::move(callback)));
}

void CopyOrMoveOperationDelegate::PostProcessDirectoryAfterTouchFile(
    const FileSystemURL& src_url,
    StatusCallback callback,
    base::File::Error error) {
  // Even if the TouchFile is failed, just ignore it.

  if (operation_type_ == OPERATION_COPY) {
    std::move(callback).Run(base::File::FILE_OK);
    return;
  }

  DCHECK_EQ(OPERATION_MOVE, operation_type_);

  // All files and subdirectories in the directory should be moved here,
  // so remove the source directory for finalizing move operation.
  operation_runner()->Remove(
      src_url, false /* recursive */,
      base::BindOnce(&CopyOrMoveOperationDelegate::DidRemoveSourceForMove,
                     weak_factory_.GetWeakPtr(), src_url, std::move(callback)));
}

void CopyOrMoveOperationDelegate::DidRemoveSourceForMove(
    const FileSystemURL& src_url,
    StatusCallback callback,
    base::File::Error error) {
  if (error != base::File::FILE_OK &&
      error != base::File::FILE_ERROR_NOT_FOUND) {
    RunCopyOrMoveHookDelegateCallbackLater(
        &CopyOrMoveHookDelegate::OnError, src_url, FileSystemURL(), error,
        CreateErrorSuppressCallback(std::move(callback), error));
    return;
  }
  RunCopyOrMoveHookDelegateCallbackLater(
      &CopyOrMoveHookDelegate::OnEndRemoveSource, src_url);
  std::move(callback).Run(error);
}

FileSystemURL CopyOrMoveOperationDelegate::CreateDestURL(
    const FileSystemURL& src_url) const {
  DCHECK_EQ(src_root_.type(), src_url.type());
  DCHECK_EQ(src_root_.origin(), src_url.origin());

  base::FilePath relative = dest_root_.virtual_path();
  src_root_.virtual_path().AppendRelativePath(src_url.virtual_path(),
                                              &relative);
  FileSystemURL dest_url = file_system_context()->CreateCrackedFileSystemURL(
      dest_root_.storage_key(), dest_root_.mount_type(), relative);
  if (dest_root_.bucket().has_value())
    dest_url.SetBucket(dest_root_.bucket().value());
  return dest_url;
}

}  // namespace storage

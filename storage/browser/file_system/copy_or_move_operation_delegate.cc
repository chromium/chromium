// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/copy_or_move_operation_delegate.h"

#include <stdint.h>

#include <memory>
#include <tuple>
#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "storage/browser/blob/shareable_file_reference.h"
#include "storage/browser/file_system/copy_or_move_file_validator.h"
#include "storage/browser/file_system/file_observers.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "storage/browser/file_system/file_stream_writer.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/common/file_system/file_system_util.h"

namespace storage {

const int64_t kFlushIntervalInBytes = 10 << 20;  // 10MB.

class CopyOrMoveOperationDelegate::CopyOrMoveImpl {
 public:
  virtual ~CopyOrMoveImpl() = default;
  virtual void Run(CopyOrMoveOperationDelegate::StatusCallback callback) = 0;
  virtual void Cancel() = 0;

 protected:
  CopyOrMoveImpl() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(CopyOrMoveImpl);
};

namespace {

// Copies a file on a (same) file system. Just delegate the operation to
// |operation_runner|.
class CopyOrMoveOnSameFileSystemImpl
    : public CopyOrMoveOperationDelegate::CopyOrMoveImpl {
 public:
  CopyOrMoveOnSameFileSystemImpl(
      FileSystemOperationRunner* operation_runner,
      CopyOrMoveOperationDelegate::OperationType operation_type,
      const FileSystemURL& src_url,
      const FileSystemURL& dest_url,
      CopyOrMoveOperationDelegate::CopyOrMoveOption option,
      FileSystemOperation::CopyFileProgressCallback file_progress_callback)
      : operation_runner_(operation_runner),
        operation_type_(operation_type),
        src_url_(src_url),
        dest_url_(dest_url),
        option_(option),
        file_progress_callback_(std::move(file_progress_callback)) {}

  void Run(CopyOrMoveOperationDelegate::StatusCallback callback) override {
    if (operation_type_ == CopyOrMoveOperationDelegate::OPERATION_MOVE) {
      operation_runner_->MoveFileLocal(src_url_, dest_url_, option_,
                                       std::move(callback));
    } else {
      operation_runner_->CopyFileLocal(src_url_, dest_url_, option_,
                                       file_progress_callback_,
                                       std::move(callback));
    }
  }

  void Cancel() override {
    // We can do nothing for the copy/move operation on a local file system.
    // Assuming the operation is quickly done, it should be ok to just wait
    // for the completion.
  }

 private:
  FileSystemOperationRunner* operation_runner_;
  CopyOrMoveOperationDelegate::OperationType operation_type_;
  FileSystemURL src_url_;
  FileSystemURL dest_url_;
  CopyOrMoveOperationDelegate::CopyOrMoveOption option_;
  FileSystemOperation::CopyFileProgressCallback file_progress_callback_;
  DISALLOW_COPY_AND_ASSIGN(CopyOrMoveOnSameFileSystemImpl);
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
      CopyOrMoveOperationDelegate::CopyOrMoveOption option,
      CopyOrMoveFileValidatorFactory* validator_factory,
      const FileSystemOperation::CopyFileProgressCallback&
          file_progress_callback)
      : operation_runner_(operation_runner),
        operation_type_(operation_type),
        src_url_(src_url),
        dest_url_(dest_url),
        option_(option),
        validator_factory_(validator_factory),
        file_progress_callback_(file_progress_callback),
        cancel_requested_(false) {}

  void Run(CopyOrMoveOperationDelegate::StatusCallback callback) override {
    file_progress_callback_.Run(0);
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
      scoped_refptr<storage::ShareableFileReference> file_ref) {
    if (cancel_requested_)
      error = base::File::FILE_ERROR_ABORT;

    if (error != base::File::FILE_OK) {
      std::move(callback).Run(error);
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
      scoped_refptr<storage::ShareableFileReference> file_ref,
      CopyOrMoveOperationDelegate::StatusCallback callback,
      base::File::Error error) {
    if (cancel_requested_)
      error = base::File::FILE_ERROR_ABORT;

    if (error != base::File::FILE_OK) {
      std::move(callback).Run(error);
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
      scoped_refptr<storage::ShareableFileReference> file_ref,
      CopyOrMoveOperationDelegate::StatusCallback callback,
      base::File::Error error) {
    if (cancel_requested_)
      error = base::File::FILE_ERROR_ABORT;

    if (error != base::File::FILE_OK) {
      std::move(callback).Run(error);
      return;
    }

    file_progress_callback_.Run(file_info.size);

    if (option_ == FileSystemOperation::OPTION_NONE) {
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
    // Even if TouchFile is failed, just ignore it.

    if (cancel_requested_) {
      std::move(callback).Run(base::File::FILE_ERROR_ABORT);
      return;
    }

    // |validator_| is nullptr when the destination filesystem does not do
    // validation.
    if (!validator_) {
      // No validation is needed.
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
    if (cancel_requested_) {
      std::move(callback).Run(base::File::FILE_ERROR_ABORT);
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
      std::move(callback).Run(base::File::FILE_OK);
      return;
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
    std::move(callback).Run(error);
  }

  void DidRemoveDestForError(
      base::File::Error prior_error,
      CopyOrMoveOperationDelegate::StatusCallback callback,
      base::File::Error error) {
    if (error != base::File::FILE_OK) {
      VLOG(1) << "Error removing destination file after validation error: "
              << error;
    }
    std::move(callback).Run(prior_error);
  }

  // Runs pre-write validation.
  void PreWriteValidation(
      const base::FilePath& platform_path,
      CopyOrMoveOperationDelegate::StatusCallback callback) {
    DCHECK(validator_factory_);
    validator_.reset(validator_factory_->CreateCopyOrMoveFileValidator(
        src_url_, platform_path));
    // TODO(mek): Update CopyOrMoveFileValidator to use OnceCallback.
    validator_->StartPreWriteValidation(
        base::AdaptCallbackForRepeating(std::move(callback)));
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
      scoped_refptr<storage::ShareableFileReference> file_ref) {
    if (cancel_requested_)
      error = base::File::FILE_ERROR_ABORT;

    if (error != base::File::FILE_OK) {
      std::move(callback).Run(error);
      return;
    }

    DCHECK(validator_);
    // Note: file_ref passed here to keep the file alive until after
    // the StartPostWriteValidation operation finishes.
    // TODO(mek): Update CopyOrMoveFileValidator to use OnceCallback.
    validator_->StartPostWriteValidation(
        platform_path, base::AdaptCallbackForRepeating(base::BindOnce(
                           &SnapshotCopyOrMoveImpl::DidPostWriteValidation,
                           weak_factory_.GetWeakPtr(), std::move(file_ref),
                           std::move(callback))));
  }

  // |file_ref| is unused; it is passed here to make sure the reference is
  // alive until after post-write validation is complete.
  void DidPostWriteValidation(
      scoped_refptr<storage::ShareableFileReference> file_ref,
      CopyOrMoveOperationDelegate::StatusCallback callback,
      base::File::Error error) {
    std::move(callback).Run(error);
  }

  FileSystemOperationRunner* operation_runner_;
  CopyOrMoveOperationDelegate::OperationType operation_type_;
  FileSystemURL src_url_;
  FileSystemURL dest_url_;

  CopyOrMoveOperationDelegate::CopyOrMoveOption option_;
  CopyOrMoveFileValidatorFactory* validator_factory_;
  std::unique_ptr<CopyOrMoveFileValidator> validator_;
  FileSystemOperation::CopyFileProgressCallback file_progress_callback_;
  bool cancel_requested_;
  base::WeakPtrFactory<SnapshotCopyOrMoveImpl> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(SnapshotCopyOrMoveImpl);
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
      CopyOrMoveOperationDelegate::CopyOrMoveOption option,
      std::unique_ptr<storage::FileStreamReader> reader,
      std::unique_ptr<FileStreamWriter> writer,
      const FileSystemOperation::CopyFileProgressCallback&
          file_progress_callback)
      : operation_runner_(operation_runner),
        file_system_context_(file_system_context),
        operation_type_(operation_type),
        src_url_(src_url),
        dest_url_(dest_url),
        option_(option),
        reader_(std::move(reader)),
        writer_(std::move(writer)),
        file_progress_callback_(file_progress_callback),
        cancel_requested_(false) {}

  void Run(CopyOrMoveOperationDelegate::StatusCallback callback) override {
    // Reader can be created even if the entry does not exist or the entry is
    // a directory. To check errors before destination file creation,
    // check metadata first.
    operation_runner_->GetMetadata(
        src_url_,
        FileSystemOperation::GET_METADATA_FIELD_IS_DIRECTORY |
            FileSystemOperation::GET_METADATA_FIELD_LAST_MODIFIED,
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
      std::move(callback).Run(error);
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
      std::move(callback).Run(error);
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
      std::move(callback).Run(error);
      return;
    }

    NotifyOnStartUpdate(dest_url_);
    DCHECK(!copy_helper_);
    copy_helper_.reset(new CopyOrMoveOperationDelegate::StreamCopyHelper(
        std::move(reader_), std::move(writer_),
        dest_url_.mount_option().flush_policy(), kReadBufferSize,
        file_progress_callback_,
        base::TimeDelta::FromMilliseconds(
            kMinProgressCallbackInvocationSpanInMilliseconds)));
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
    if (cancel_requested_)
      error = base::File::FILE_ERROR_ABORT;

    if (error != base::File::FILE_OK) {
      std::move(callback).Run(error);
      return;
    }

    if (option_ == FileSystemOperation::OPTION_NONE) {
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
    // Even if TouchFile is failed, just ignore it.
    if (cancel_requested_) {
      std::move(callback).Run(base::File::FILE_ERROR_ABORT);
      return;
    }

    if (operation_type_ == CopyOrMoveOperationDelegate::OPERATION_COPY) {
      std::move(callback).Run(base::File::FILE_OK);
      return;
    }

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
    std::move(callback).Run(error);
  }

  FileSystemOperationRunner* operation_runner_;
  scoped_refptr<FileSystemContext> file_system_context_;
  CopyOrMoveOperationDelegate::OperationType operation_type_;
  FileSystemURL src_url_;
  FileSystemURL dest_url_;
  CopyOrMoveOperationDelegate::CopyOrMoveOption option_;
  std::unique_ptr<storage::FileStreamReader> reader_;
  std::unique_ptr<FileStreamWriter> writer_;
  FileSystemOperation::CopyFileProgressCallback file_progress_callback_;
  std::unique_ptr<CopyOrMoveOperationDelegate::StreamCopyHelper> copy_helper_;
  bool cancel_requested_;
  base::WeakPtrFactory<StreamCopyOrMoveImpl> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(StreamCopyOrMoveImpl);
};

}  // namespace

CopyOrMoveOperationDelegate::StreamCopyHelper::StreamCopyHelper(
    std::unique_ptr<storage::FileStreamReader> reader,
    std::unique_ptr<FileStreamWriter> writer,
    storage::FlushPolicy flush_policy,
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
    if (flush_policy_ == storage::FlushPolicy::FLUSH_ON_COMPLETION)
      Flush(true /* is_eof */);
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
  base::Time now = base::Time::Now();
  if (now - last_progress_callback_invocation_time_ >=
      min_progress_callback_invocation_span_) {
    file_progress_callback_.Run(num_copied_bytes_);
    last_progress_callback_invocation_time_ = now;
  }

  if (buffer->BytesRemaining() > 0) {
    Write(buffer);
    return;
  }

  if (flush_policy_ == storage::FlushPolicy::FLUSH_ON_COMPLETION &&
      (num_copied_bytes_ - previous_flush_offset_) > kFlushIntervalInBytes) {
    Flush(false /* not is_eof */);
  } else {
    Read();
  }
}

void CopyOrMoveOperationDelegate::StreamCopyHelper::Flush(bool is_eof) {
  int result = writer_->Flush(base::BindOnce(
      &StreamCopyHelper::DidFlush, weak_factory_.GetWeakPtr(), is_eof));
  if (result != net::ERR_IO_PENDING)
    DidFlush(is_eof, result);
}

void CopyOrMoveOperationDelegate::StreamCopyHelper::DidFlush(bool is_eof,
                                                             int result) {
  if (cancel_requested_) {
    std::move(completion_callback_).Run(base::File::FILE_ERROR_ABORT);
    return;
  }

  previous_flush_offset_ = num_copied_bytes_;
  if (is_eof)
    std::move(completion_callback_).Run(NetErrorToFileError(result));
  else
    Read();
}

CopyOrMoveOperationDelegate::CopyOrMoveOperationDelegate(
    FileSystemContext* file_system_context,
    const FileSystemURL& src_root,
    const FileSystemURL& dest_root,
    OperationType operation_type,
    CopyOrMoveOption option,
    ErrorBehavior error_behavior,
    const CopyProgressCallback& progress_callback,
    StatusCallback callback)
    : RecursiveOperationDelegate(file_system_context),
      src_root_(src_root),
      dest_root_(dest_root),
      operation_type_(operation_type),
      option_(option),
      error_behavior_(error_behavior),
      progress_callback_(progress_callback),
      callback_(std::move(callback)) {
  same_file_system_ = src_root_.IsInSameFileSystem(dest_root_);
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
  StartRecursiveOperation(src_root_, error_behavior_, std::move(callback_));
}

void CopyOrMoveOperationDelegate::ProcessFile(const FileSystemURL& src_url,
                                              StatusCallback callback) {
  if (!progress_callback_.is_null()) {
    progress_callback_.Run(FileSystemOperation::BEGIN_COPY_ENTRY, src_url,
                           FileSystemURL(), 0);
  }

  FileSystemURL dest_url = CreateDestURL(src_url);
  std::unique_ptr<CopyOrMoveImpl> impl;
  if (same_file_system_ &&
      (file_system_context()
           ->GetFileSystemBackend(src_url.type())
           ->HasInplaceCopyImplementation(src_url.type()) ||
       operation_type_ == OPERATION_MOVE)) {
    impl = std::make_unique<CopyOrMoveOnSameFileSystemImpl>(
        operation_runner(), operation_type_, src_url, dest_url, option_,
        base::BindRepeating(&CopyOrMoveOperationDelegate::OnCopyFileProgress,
                            weak_factory_.GetWeakPtr(), src_url));
  } else {
    // Cross filesystem case.
    base::File::Error error = base::File::FILE_ERROR_FAILED;
    CopyOrMoveFileValidatorFactory* validator_factory =
        file_system_context()->GetCopyOrMoveFileValidatorFactory(
            dest_root_.type(), &error);
    if (error != base::File::FILE_OK) {
      if (!progress_callback_.is_null())
        progress_callback_.Run(FileSystemOperation::ERROR_COPY_ENTRY, src_url,
                               dest_url, 0);

      std::move(callback).Run(error);
      return;
    }

    if (!validator_factory) {
      std::unique_ptr<storage::FileStreamReader> reader =
          file_system_context()->CreateFileStreamReader(
              src_url, 0 /* offset */, storage::kMaximumLength, base::Time());
      std::unique_ptr<FileStreamWriter> writer =
          file_system_context()->CreateFileStreamWriter(dest_url, 0);
      if (reader && writer) {
        impl = std::make_unique<StreamCopyOrMoveImpl>(
            operation_runner(), file_system_context(), operation_type_, src_url,
            dest_url, option_, std::move(reader), std::move(writer),
            base::BindRepeating(
                &CopyOrMoveOperationDelegate::OnCopyFileProgress,
                weak_factory_.GetWeakPtr(), src_url));
      }
    }

    if (!impl) {
      impl = std::make_unique<SnapshotCopyOrMoveImpl>(
          operation_runner(), operation_type_, src_url, dest_url, option_,
          validator_factory,
          base::BindRepeating(&CopyOrMoveOperationDelegate::OnCopyFileProgress,
                              weak_factory_.GetWeakPtr(), src_url));
    }
  }

  // Register the running task.

  CopyOrMoveImpl* impl_ptr = impl.get();
  running_copy_set_[impl_ptr] = std::move(impl);
  impl_ptr->Run(base::BindOnce(&CopyOrMoveOperationDelegate::DidCopyOrMoveFile,
                               weak_factory_.GetWeakPtr(), src_url, dest_url,
                               std::move(callback), impl_ptr));
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

  if (!progress_callback_.is_null()) {
    progress_callback_.Run(FileSystemOperation::BEGIN_COPY_ENTRY, src_url,
                           FileSystemURL(), 0);
  }

  ProcessDirectoryInternal(src_url, CreateDestURL(src_url),
                           std::move(callback));
}

void CopyOrMoveOperationDelegate::PostProcessDirectory(
    const FileSystemURL& src_url,
    StatusCallback callback) {
  if (option_ == FileSystemOperation::OPTION_NONE) {
    PostProcessDirectoryAfterTouchFile(src_url, std::move(callback),
                                       base::File::FILE_OK);
    return;
  }

  operation_runner()->GetMetadata(
      src_url, FileSystemOperation::GET_METADATA_FIELD_LAST_MODIFIED,
      base::BindOnce(
          &CopyOrMoveOperationDelegate::PostProcessDirectoryAfterGetMetadata,
          weak_factory_.GetWeakPtr(), src_url, std::move(callback)));
}

void CopyOrMoveOperationDelegate::OnCancel() {
  // Request to cancel all running Copy/Move file.
  for (auto& job : running_copy_set_)
    job.first->Cancel();
}

void CopyOrMoveOperationDelegate::DidCopyOrMoveFile(
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    StatusCallback callback,
    CopyOrMoveImpl* impl,
    base::File::Error error) {
  running_copy_set_.erase(impl);

  if (!progress_callback_.is_null() && error != base::File::FILE_OK &&
      error != base::File::FILE_ERROR_NOT_A_FILE)
    progress_callback_.Run(FileSystemOperation::ERROR_COPY_ENTRY, src_url,
                           dest_url, 0);

  if (!progress_callback_.is_null() && error == base::File::FILE_OK) {
    progress_callback_.Run(FileSystemOperation::END_COPY_ENTRY, src_url,
                           dest_url, 0);
  }

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

  ProcessDirectoryInternal(src_root_, dest_root_, std::move(callback));
}

void CopyOrMoveOperationDelegate::ProcessDirectoryInternal(
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    StatusCallback callback) {
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
  if (!progress_callback_.is_null() && error == base::File::FILE_OK) {
    progress_callback_.Run(FileSystemOperation::END_COPY_ENTRY, src_url,
                           dest_url, 0);
  }

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
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void CopyOrMoveOperationDelegate::DidRemoveSourceForMove(
    StatusCallback callback,
    base::File::Error error) {
  if (error == base::File::FILE_ERROR_NOT_FOUND)
    error = base::File::FILE_OK;
  std::move(callback).Run(error);
}

void CopyOrMoveOperationDelegate::OnCopyFileProgress(
    const FileSystemURL& src_url,
    int64_t size) {
  if (!progress_callback_.is_null()) {
    progress_callback_.Run(FileSystemOperation::PROGRESS, src_url,
                           FileSystemURL(), size);
  }
}

FileSystemURL CopyOrMoveOperationDelegate::CreateDestURL(
    const FileSystemURL& src_url) const {
  DCHECK_EQ(src_root_.type(), src_url.type());
  DCHECK_EQ(src_root_.origin(), src_url.origin());

  base::FilePath relative = dest_root_.virtual_path();
  src_root_.virtual_path().AppendRelativePath(src_url.virtual_path(),
                                              &relative);
  return file_system_context()->CreateCrackedFileSystemURL(
      dest_root_.origin().GetURL(), dest_root_.mount_type(), relative);
}

}  // namespace storage

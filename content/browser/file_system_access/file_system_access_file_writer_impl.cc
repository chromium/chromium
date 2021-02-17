// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_file_writer_impl.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/services/quarantine/quarantine.h"
#include "content/browser/file_system_access/file_system_access_error.h"
#include "content/browser/file_system_access/file_system_access_manager_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "crypto/secure_hash.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_error.mojom.h"

using blink::mojom::FileSystemAccessStatus;
using storage::FileSystemOperation;
using storage::FileSystemOperationRunner;

namespace content {

namespace {

// For after write checks we need the hash and size of the file. That data is
// calculated on the IO thread by this class.
// This class is ref-counted to make it easier to integrate with the
// FileStreamReader API where methods either return synchronously or invoke
// their callback asynchronously.
class HashCalculator : public base::RefCounted<HashCalculator> {
 public:
  // Must be called on the FileSystemContext's IO runner.
  static void CreateAndStart(
      scoped_refptr<storage::FileSystemContext> context,
      FileSystemAccessFileWriterImpl::HashCallback callback,
      const storage::FileSystemURL& swap_url,
      storage::FileSystemOperationRunner*) {
    auto calculator = base::MakeRefCounted<HashCalculator>(std::move(context),
                                                           std::move(callback));
    calculator->Start(swap_url);
  }

  HashCalculator(scoped_refptr<storage::FileSystemContext> context,
                 FileSystemAccessFileWriterImpl::HashCallback callback)
      : context_(std::move(context)), callback_(std::move(callback)) {
    DCHECK(context_);
  }

 private:
  friend class base::RefCounted<HashCalculator>;
  ~HashCalculator() = default;

  void Start(const storage::FileSystemURL& swap_url) {
    reader_ = context_->CreateFileStreamReader(
        swap_url, 0, storage::kMaximumLength, base::Time());
    int64_t length =
        reader_->GetLength(base::BindOnce(&HashCalculator::GotLength, this));
    if (length == net::ERR_IO_PENDING)
      return;
    GotLength(length);
  }

  void GotLength(int64_t length) {
    if (length < 0) {
      std::move(callback_).Run(storage::NetErrorToFileError(length),
                               std::string(), -1);
      return;
    }

    file_size_ = length;
    ReadMore();
  }

  void ReadMore() {
    DCHECK_GE(file_size_, 0);
    int read_result =
        reader_->Read(buffer_.get(), buffer_->size(),
                      base::BindOnce(&HashCalculator::DidRead, this));
    if (read_result == net::ERR_IO_PENDING)
      return;
    DidRead(read_result);
  }

  void DidRead(int bytes_read) {
    DCHECK_GE(file_size_, 0);
    if (bytes_read < 0) {
      std::move(callback_).Run(storage::NetErrorToFileError(bytes_read),
                               std::string(), -1);
      return;
    }
    if (bytes_read == 0) {
      std::string hash_str(hash_->GetHashLength(), 0);
      hash_->Finish(base::data(hash_str), hash_str.size());
      std::move(callback_).Run(base::File::FILE_OK, hash_str, file_size_);
      return;
    }

    hash_->Update(buffer_->data(), bytes_read);
    ReadMore();
  }

  const scoped_refptr<storage::FileSystemContext> context_;
  FileSystemAccessFileWriterImpl::HashCallback callback_;

  const scoped_refptr<net::IOBufferWithSize> buffer_{
      base::MakeRefCounted<net::IOBufferWithSize>(8 * 1024)};

  const std::unique_ptr<crypto::SecureHash> hash_{
      crypto::SecureHash::Create(crypto::SecureHash::SHA256)};

  std::unique_ptr<storage::FileStreamReader> reader_;
  int64_t file_size_ = -1;
};

void RemoveSwapFile(const storage::FileSystemURL& swap_url,
                    storage::FileSystemOperationRunner* runner) {
  runner->Remove(swap_url, /*recursive=*/false, base::DoNothing());
}

}  // namespace

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
    const SharedHandleState& handle_state,
    mojo::PendingReceiver<blink::mojom::FileSystemAccessFileWriter> receiver,
    bool has_transient_user_activation,
    bool auto_close,
    download::QuarantineConnectionCallback quarantine_connection_callback)
    : FileSystemAccessHandleBase(manager, context, url, handle_state),
      receiver_(this, std::move(receiver)),
      swap_url_(swap_url),
      quarantine_connection_callback_(
          std::move(quarantine_connection_callback)),
      has_transient_user_activation_(has_transient_user_activation),
      auto_close_(auto_close) {
  DCHECK_EQ(swap_url.type(), url.type());
  receiver_.set_disconnect_handler(base::BindOnce(
      &FileSystemAccessFileWriterImpl::OnDisconnect, base::Unretained(this)));
}

FileSystemAccessFileWriterImpl::~FileSystemAccessFileWriterImpl() {
  // Purge the swap file. The swap file should be deleted after Close(), but
  // we'll try to delete it anyways in case the writer wasn't closed cleanly.
  DoFileSystemOperation(
      FROM_HERE, &FileSystemOperationRunner::RemoveFile,
      base::BindOnce(
          [](const storage::FileSystemURL& swap_url, base::File::Error result) {
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

// Do not call this method if |close_callback_| is not set.
void FileSystemAccessFileWriterImpl::CallCloseCallbackAndDeleteThis(
    blink::mojom::FileSystemAccessErrorPtr result) {
  std::move(close_callback_).Run(std::move(result));

  // |this| is deleted after this call.
  manager()->RemoveFileWriter(this);
}

void FileSystemAccessFileWriterImpl::OnDisconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  receiver_.reset();

  if (is_close_pending())
    // Mojo connection lost while Close() in progress.
    return;

  if (auto_close_) {
    // Close the Writer. |this| is deleted via
    // CallCloseCallbackAndDeleteThis when Close() finishes.
    Close(base::BindOnce([](blink::mojom::FileSystemAccessErrorPtr result) {
      if (result->status != blink::mojom::FileSystemAccessStatus::kOk) {
        DLOG(ERROR) << "AutoClose failed with result:"
                    << base::File::ErrorToString(result->file_error);
      }
    }));
    return;
  }

  // Mojo connection severed before Close() called. Destroy |this|.
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

  DoFileSystemOperation(
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

  DoFileSystemOperation(
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

  if (!RequireSecurityChecks() || !manager()->permission_context()) {
    DidAfterWriteCheck(
        FileSystemAccessPermissionContext::AfterWriteCheckResult::kAllow);
    return;
  }

  ComputeHashForSwapFile(
      base::BindOnce(&FileSystemAccessFileWriterImpl::DoAfterWriteCheck,
                     weak_factory_.GetWeakPtr()));
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

  // |this| is deleted after this call.
  manager()->RemoveFileWriter(this);
}

// static
void FileSystemAccessFileWriterImpl::DoAfterWriteCheck(
    base::File::Error hash_result,
    const std::string& hash,
    int64_t size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (hash_result != base::File::FILE_OK) {
    // Calculating the hash failed try deleting the swap file and invoke the
    // callback.
    manager()->operation_runner().PostTaskWithThisObject(
        FROM_HERE, base::BindOnce(&RemoveSwapFile, swap_url()));
    CallCloseCallbackAndDeleteThis(file_system_access_error::FromStatus(
        FileSystemAccessStatus::kOperationAborted,
        "Failed to perform Safe Browsing check."));
    return;
  }

  auto item = std::make_unique<FileSystemAccessWriteItem>();
  item->target_file_path = url().path();
  item->full_path = swap_url().path();
  item->sha256_hash = hash;
  item->size = size;
  item->frame_url = context().url;
  item->has_user_gesture = has_transient_user_activation_;
  manager()->permission_context()->PerformAfterWriteChecks(
      std::move(item), context().frame_id,
      base::BindOnce(&FileSystemAccessFileWriterImpl::DidAfterWriteCheck,
                     weak_factory_.GetWeakPtr()));
}

void FileSystemAccessFileWriterImpl::DidAfterWriteCheck(
    FileSystemAccessPermissionContext::AfterWriteCheckResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (result !=
      FileSystemAccessPermissionContext::AfterWriteCheckResult::kAllow) {
    // Safe browsing check failed. In this case we should try deleting the swap
    // file and call the callback to report that close failed.
    manager()->operation_runner().PostTaskWithThisObject(
        FROM_HERE, base::BindOnce(&RemoveSwapFile, swap_url()));
    CallCloseCallbackAndDeleteThis(file_system_access_error::FromStatus(
        FileSystemAccessStatus::kOperationAborted,
        "Write operation blocked by Safe Browsing."));
    return;
  }

  // If the move operation succeeds, the path pointing to the swap file
  // will not exist anymore.
  // In case of error, the swap file URL will point to a valid filesystem
  // location. The file at this URL will be deleted when the mojo pipe closes.
  base::OnceCallback<void(base::File::Error)> result_callback;
  if (RequireSecurityChecks()) {
    GURL referrer_url = manager()->is_off_the_record() ? GURL() : context().url;
    mojo::Remote<quarantine::mojom::Quarantine> quarantine_remote;
    if (quarantine_connection_callback_) {
      quarantine_connection_callback_.Run(
          quarantine_remote.BindNewPipeAndPassReceiver());
    }
    result_callback =
        base::BindOnce(&FileSystemAccessFileWriterImpl::DidSwapFileDoQuarantine,
                       weak_factory_.GetWeakPtr(), url(), referrer_url,
                       std::move(quarantine_remote));
  } else {
    result_callback = base::BindOnce(
        &FileSystemAccessFileWriterImpl::DidSwapFileSkipQuarantine,
        weak_factory_.GetWeakPtr());
  }
  DoFileSystemOperation(
      FROM_HERE, &FileSystemOperationRunner::MoveFileLocal,
      std::move(result_callback), swap_url(), url(),
      storage::FileSystemOperation::OPTION_PRESERVE_LAST_MODIFIED);
}

void FileSystemAccessFileWriterImpl::DidSwapFileSkipQuarantine(
    base::File::Error result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (result != base::File::FILE_OK) {
    DLOG(ERROR) << "Swap file move operation failed source: "
                << swap_url().path() << " dest: " << url().path()
                << " error: " << base::File::ErrorToString(result);
    CallCloseCallbackAndDeleteThis(
        file_system_access_error::FromFileError(result));
    return;
  }

  CallCloseCallbackAndDeleteThis(file_system_access_error::Ok());
}

void FileSystemAccessFileWriterImpl::DidSwapFileDoQuarantine(
    const storage::FileSystemURL& target_url,
    const GURL& referrer_url,
    mojo::Remote<quarantine::mojom::Quarantine> quarantine_remote,
    base::File::Error result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (result != base::File::FILE_OK) {
    DLOG(ERROR) << "Swap file move operation failed dest: " << target_url.path()
                << " error: " << base::File::ErrorToString(result);
    CallCloseCallbackAndDeleteThis(
        file_system_access_error::FromFileError(result));
    return;
  }

  // The quarantine service operates on files identified by a base::FilePath. As
  // such we can only quarantine files that are actual local files.
  // On ChromeOS on the other hand anything that isn't in the sandboxed file
  // system is also uniquely identifiable by its FileSystemURL::path(), and
  // thus we accept all other FileSystemURL types.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  DCHECK(target_url.type() != storage::kFileSystemTypeTemporary &&
         target_url.type() != storage::kFileSystemTypePersistent)
      << target_url.type();
#else
  DCHECK(target_url.type() == storage::kFileSystemTypeLocal ||
         target_url.type() == storage::kFileSystemTypeTest)
      << target_url.type();
#endif

  GURL authority_url =
      referrer_url.is_valid() && referrer_url.SchemeIsHTTPOrHTTPS()
          ? referrer_url
          : GURL();

  if (quarantine_remote) {
    quarantine::mojom::Quarantine* raw_quarantine = quarantine_remote.get();
    raw_quarantine->QuarantineFile(
        target_url.path(), authority_url, referrer_url,
        GetContentClient()
            ->browser()
            ->GetApplicationClientGUIDForQuarantineCheck(),
        mojo::WrapCallbackWithDefaultInvokeIfNotRun(
            base::BindOnce(&FileSystemAccessFileWriterImpl::DidAnnotateFile,
                           weak_factory_.GetWeakPtr(),
                           std::move(quarantine_remote)),
            quarantine::mojom::QuarantineFileResult::ANNOTATION_FAILED));
  } else {
#if defined(OS_WIN)
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&quarantine::SetInternetZoneIdentifierDirectly,
                       target_url.path(), authority_url, referrer_url),
        base::BindOnce(&FileSystemAccessFileWriterImpl::DidAnnotateFile,
                       weak_factory_.GetWeakPtr(),
                       std::move(quarantine_remote)));
#else
    DidAnnotateFile(std::move(quarantine_remote),
                    quarantine::mojom::QuarantineFileResult::ANNOTATION_FAILED);
#endif
  }
}

void FileSystemAccessFileWriterImpl::DidAnnotateFile(
    mojo::Remote<quarantine::mojom::Quarantine> quarantine_remote,
    quarantine::mojom::QuarantineFileResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (result != quarantine::mojom::QuarantineFileResult::OK &&
      result != quarantine::mojom::QuarantineFileResult::ANNOTATION_FAILED) {
    // If malware was detected, or the file referrer was blocked by policy, the
    // file will be deleted at this point by AttachmentServices on Windows.
    // There is nothing to do except to return the error message to the
    // application.
    CallCloseCallbackAndDeleteThis(file_system_access_error::FromStatus(
        FileSystemAccessStatus::kOperationAborted,
        "Write operation aborted due to security policy."));
    return;
  }

  CallCloseCallbackAndDeleteThis(file_system_access_error::Ok());
}

void FileSystemAccessFileWriterImpl::ComputeHashForSwapFile(
    HashCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto wrapped_callback = base::BindOnce(
      [](scoped_refptr<base::SequencedTaskRunner> runner, HashCallback callback,
         base::File::Error error, const std::string& hash, int64_t size) {
        runner->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback), error, hash, size));
      },
      base::SequencedTaskRunnerHandle::Get(), std::move(callback));

  manager()->operation_runner().PostTaskWithThisObject(
      FROM_HERE, base::BindOnce(&HashCalculator::CreateAndStart,
                                base::WrapRefCounted(file_system_context()),
                                std::move(wrapped_callback), swap_url()));
}

base::WeakPtr<FileSystemAccessHandleBase>
FileSystemAccessFileWriterImpl::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace content

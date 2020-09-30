// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/native_file_system_file_writer_impl.h"

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "components/services/quarantine/quarantine.h"
#include "content/browser/file_system_access/native_file_system_error.h"
#include "content/browser/file_system_access/native_file_system_manager_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "crypto/secure_hash.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "third_party/blink/public/common/blob/blob_utils.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/native_file_system_error.mojom.h"

using blink::mojom::NativeFileSystemStatus;
using storage::BlobDataHandle;
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
      NativeFileSystemFileWriterImpl::HashCallback callback,
      const storage::FileSystemURL& swap_url,
      storage::FileSystemOperationRunner*) {
    auto calculator = base::MakeRefCounted<HashCalculator>(std::move(context),
                                                           std::move(callback));
    calculator->Start(swap_url);
  }

  HashCalculator(scoped_refptr<storage::FileSystemContext> context,
                 NativeFileSystemFileWriterImpl::HashCallback callback)
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
  NativeFileSystemFileWriterImpl::HashCallback callback_;

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

struct NativeFileSystemFileWriterImpl::WriteState {
  WriteCallback callback;
  uint64_t bytes_written = 0;
};

NativeFileSystemFileWriterImpl::NativeFileSystemFileWriterImpl(
    NativeFileSystemManagerImpl* manager,
    const BindingContext& context,
    const storage::FileSystemURL& url,
    const storage::FileSystemURL& swap_url,
    const SharedHandleState& handle_state,
    bool has_transient_user_activation,
    download::QuarantineConnectionCallback quarantine_connection_callback)
    : NativeFileSystemHandleBase(manager, context, url, handle_state),
      swap_url_(swap_url),
      quarantine_connection_callback_(
          std::move(quarantine_connection_callback)),
      has_transient_user_activation_(has_transient_user_activation) {
  DCHECK_EQ(swap_url.type(), url.type());
}

NativeFileSystemFileWriterImpl::~NativeFileSystemFileWriterImpl() {
  if (can_purge()) {
    DoFileSystemOperation(FROM_HERE, &FileSystemOperationRunner::RemoveFile,
                          base::BindOnce(
                              [](const storage::FileSystemURL& swap_url,
                                 base::File::Error result) {
                                if (result != base::File::FILE_OK) {
                                  DLOG(ERROR)
                                      << "Error Deleting Swap File, status: "
                                      << base::File::ErrorToString(result)
                                      << " path: " << swap_url.path();
                                }
                              },
                              swap_url()),
                          swap_url());
  }
}

void NativeFileSystemFileWriterImpl::Write(
    uint64_t offset,
    mojo::PendingRemote<blink::mojom::Blob> data,
    WriteCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  RunWithWritePermission(
      base::BindOnce(&NativeFileSystemFileWriterImpl::WriteImpl,
                     weak_factory_.GetWeakPtr(), offset, std::move(data)),
      base::BindOnce([](blink::mojom::NativeFileSystemErrorPtr result,
                        WriteCallback callback) {
        std::move(callback).Run(std::move(result),
                                /*bytes_written=*/0);
      }),
      std::move(callback));
}

void NativeFileSystemFileWriterImpl::WriteStream(
    uint64_t offset,
    mojo::ScopedDataPipeConsumerHandle stream,
    WriteStreamCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  RunWithWritePermission(
      base::BindOnce(&NativeFileSystemFileWriterImpl::WriteStreamImpl,
                     weak_factory_.GetWeakPtr(), offset, std::move(stream)),
      base::BindOnce([](blink::mojom::NativeFileSystemErrorPtr result,
                        WriteStreamCallback callback) {
        std::move(callback).Run(std::move(result),
                                /*bytes_written=*/0);
      }),
      std::move(callback));
}

void NativeFileSystemFileWriterImpl::Truncate(uint64_t length,
                                              TruncateCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  RunWithWritePermission(
      base::BindOnce(&NativeFileSystemFileWriterImpl::TruncateImpl,
                     weak_factory_.GetWeakPtr(), length),
      base::BindOnce([](blink::mojom::NativeFileSystemErrorPtr result,
                        TruncateCallback callback) {
        std::move(callback).Run(std::move(result));
      }),
      std::move(callback));
}

void NativeFileSystemFileWriterImpl::Close(CloseCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  RunWithWritePermission(
      base::BindOnce(&NativeFileSystemFileWriterImpl::CloseImpl,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce([](blink::mojom::NativeFileSystemErrorPtr result,
                        CloseCallback callback) {
        std::move(callback).Run(std::move(result));
      }),
      std::move(callback));
}

namespace {

// Writing a blob to a file consists of three operations:
// 1) The Blob reads its data, and writes it out to a mojo data pipe producer
//    handle. It calls BlobReaderClient::OnComplete when all data has been
//    written.
// 2) WriteStream reads data from the associated mojo data pipe consumer handle
//    as long as data is available.
// 3) All the read data is written to disk. Signalled by calling WriteCompleted.
//
// All of these steps are done in parallel, operating on chunks. Furthermore the
// OnComplete call from step 1) is done over a different mojo pipe than where
// the data is sent, making it possible for this to arrive either before or
// after step 3) completes. To make sure we report an error when any of these
// steps fail, this helper class waits for both the OnComplete call to arrive
// and for the write to finish before considering the entire write operation a
// success.
//
// On the other hand, as soon as we're aware of any of these steps failing, that
// error can be propagated, as that means the operation failed.
// In other words, this is like Promise.all, which resolves when all
// operations succeed, or rejects as soon as any operation fails.
//
// This class deletes itself after calling its callback.
class BlobReaderClient : public base::SupportsWeakPtr<BlobReaderClient>,
                         public blink::mojom::BlobReaderClient {
 public:
  BlobReaderClient(
      NativeFileSystemFileWriterImpl::WriteCallback callback,
      mojo::PendingReceiver<blink::mojom::BlobReaderClient> receiver)
      : callback_(std::move(callback)), receiver_(this, std::move(receiver)) {
    receiver_.set_disconnect_handler(
        base::BindOnce(&BlobReaderClient::OnDisconnect, AsWeakPtr()));
  }

  void OnCalculatedSize(uint64_t total_size,
                        uint64_t expected_content_size) override {}
  void OnComplete(int32_t status, uint64_t data_length) override {
    DCHECK(!read_result_.has_value());
    read_result_ = status;
    MaybeCallCallbackAndDeleteThis();
  }

  void WriteCompleted(blink::mojom::NativeFileSystemErrorPtr result,
                      uint64_t bytes_written) {
    DCHECK(!write_result_);
    write_result_ = std::move(result);
    bytes_written_ = bytes_written;
    MaybeCallCallbackAndDeleteThis();
  }

 private:
  friend class base::RefCounted<BlobReaderClient>;
  ~BlobReaderClient() override = default;

  void OnDisconnect() {
    if (!read_result_.has_value()) {
      // Disconnected without getting a read result, treat this as read failure.
      read_result_ = net::ERR_ABORTED;
      MaybeCallCallbackAndDeleteThis();
    }
  }

  void MaybeCallCallbackAndDeleteThis() {
    // |this| is deleted right after invoking |callback_|, so |callback_| should
    // always be valid here.
    DCHECK(callback_);

    if (read_result_.has_value() && *read_result_ != net::Error::OK) {
      // Reading from the blob failed, report that error.
      std::move(callback_).Run(native_file_system_error::FromFileError(
                                   storage::NetErrorToFileError(*read_result_)),
                               0);
      delete this;
      return;
    }
    if (!write_result_.is_null() &&
        write_result_->status != blink::mojom::NativeFileSystemStatus::kOk) {
      // Writing failed, report that error.
      std::move(callback_).Run(std::move(write_result_), 0);
      delete this;
      return;
    }
    if (read_result_.has_value() && !write_result_.is_null()) {
      // Both reading and writing succeeded, report success.
      std::move(callback_).Run(std::move(write_result_), bytes_written_);
      delete this;
      return;
    }
    // Still waiting for the other operation to complete, so don't call the
    // callback yet.
  }

  NativeFileSystemFileWriterImpl::WriteCallback callback_;
  mojo::Receiver<blink::mojom::BlobReaderClient> receiver_;

  base::Optional<int32_t> read_result_;
  blink::mojom::NativeFileSystemErrorPtr write_result_;
  uint64_t bytes_written_ = 0;
};

}  // namespace

void NativeFileSystemFileWriterImpl::WriteImpl(
    uint64_t offset,
    mojo::PendingRemote<blink::mojom::Blob> data,
    WriteCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(GetWritePermissionStatus(),
            blink::mojom::PermissionStatus::GRANTED);

  if (is_closed()) {
    std::move(callback).Run(
        native_file_system_error::FromStatus(
            NativeFileSystemStatus::kInvalidState,
            "An attempt was made to write to a closed writer."),
        /*bytes_written=*/0);
    return;
  }

  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes =
      blink::BlobUtils::GetDataPipeCapacity(blink::BlobUtils::kUnknownSize);

  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  MojoResult rv =
      mojo::CreateDataPipe(&options, &producer_handle, &consumer_handle);
  if (rv != MOJO_RESULT_OK) {
    std::move(callback).Run(
        native_file_system_error::FromStatus(
            NativeFileSystemStatus::kOperationFailed,
            "Internal read error: failed to create mojo data pipe."),
        /*bytes_written=*/0);
    return;
  }

  // TODO(mek): We can do this transformation from Blob to DataPipe in the
  // renderer, and simplify the mojom exposed interface.
  mojo::Remote<blink::mojom::Blob> blob(std::move(data));
  mojo::PendingRemote<blink::mojom::BlobReaderClient> reader_client;
  auto* client = new BlobReaderClient(
      std::move(callback), reader_client.InitWithNewPipeAndPassReceiver());
  blob->ReadAll(std::move(producer_handle), std::move(reader_client));
  WriteStreamImpl(
      offset, std::move(consumer_handle),
      base::BindOnce(&BlobReaderClient::WriteCompleted, client->AsWeakPtr()));
}

void NativeFileSystemFileWriterImpl::WriteStreamImpl(
    uint64_t offset,
    mojo::ScopedDataPipeConsumerHandle stream,
    WriteStreamCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(GetWritePermissionStatus(),
            blink::mojom::PermissionStatus::GRANTED);

  if (is_closed()) {
    std::move(callback).Run(
        native_file_system_error::FromStatus(
            NativeFileSystemStatus::kInvalidState,
            "An attempt was made to write to a closed writer."),
        /*bytes_written=*/0);
    return;
  }

  DoFileSystemOperation(
      FROM_HERE, &FileSystemOperationRunner::WriteStream,
      base::BindRepeating(&NativeFileSystemFileWriterImpl::DidWrite,
                          weak_factory_.GetWeakPtr(),
                          base::Owned(new WriteState{std::move(callback)})),
      swap_url(), std::move(stream), offset);
}

void NativeFileSystemFileWriterImpl::DidWrite(WriteState* state,
                                              base::File::Error result,
                                              int64_t bytes,
                                              bool complete) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(state);
  state->bytes_written += bytes;
  if (complete) {
    std::move(state->callback)
        .Run(native_file_system_error::FromFileError(result),
             state->bytes_written);
  }
}

void NativeFileSystemFileWriterImpl::TruncateImpl(uint64_t length,
                                                  TruncateCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(GetWritePermissionStatus(),
            blink::mojom::PermissionStatus::GRANTED);

  if (is_closed()) {
    std::move(callback).Run(native_file_system_error::FromStatus(
        NativeFileSystemStatus::kInvalidState,
        "An attempt was made to write to a closed writer."));
    return;
  }

  DoFileSystemOperation(
      FROM_HERE, &FileSystemOperationRunner::Truncate,
      base::BindOnce(
          [](TruncateCallback callback, base::File::Error result) {
            std::move(callback).Run(
                native_file_system_error::FromFileError(result));
          },
          std::move(callback)),
      swap_url(), length);
}

void NativeFileSystemFileWriterImpl::CloseImpl(CloseCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(GetWritePermissionStatus(),
            blink::mojom::PermissionStatus::GRANTED);
  if (is_closed()) {
    std::move(callback).Run(native_file_system_error::FromStatus(
        NativeFileSystemStatus::kInvalidState,
        "An attempt was made to close an already closed writer."));
    return;
  }

  // Should the writer be destructed at this point, we want to allow the
  // close operation to run its course, so we should not purge the swap file.
  // If the after write check fails, the callback for that will clean up the
  // swap file even if the writer was destroyed at that point.
  state_ = State::kClosePending;

  if (!RequireSecurityChecks() || !manager()->permission_context()) {
    DidPassAfterWriteCheck(std::move(callback));
    return;
  }

  ComputeHashForSwapFile(base::BindOnce(
      &NativeFileSystemFileWriterImpl::DoAfterWriteCheck,
      weak_factory_.GetWeakPtr(), base::WrapRefCounted(manager()), swap_url(),
      std::move(callback)));
}

// static
void NativeFileSystemFileWriterImpl::DoAfterWriteCheck(
    base::WeakPtr<NativeFileSystemFileWriterImpl> file_writer,
    scoped_refptr<NativeFileSystemManagerImpl> manager,
    const storage::FileSystemURL& swap_url,
    NativeFileSystemFileWriterImpl::CloseCallback callback,
    base::File::Error hash_result,
    const std::string& hash,
    int64_t size) {
  if (!file_writer || hash_result != base::File::FILE_OK) {
    // If writer was deleted, or calculating the hash failed try deleting the
    // swap file and invoke the callback.
    manager->operation_runner().PostTaskWithThisObject(
        FROM_HERE, base::BindOnce(&RemoveSwapFile, swap_url));
    std::move(callback).Run(native_file_system_error::FromStatus(
        NativeFileSystemStatus::kOperationAborted,
        "Failed to perform Safe Browsing check."));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(file_writer->sequence_checker_);

  auto item = std::make_unique<NativeFileSystemWriteItem>();
  item->target_file_path = file_writer->url().path();
  item->full_path = file_writer->swap_url().path();
  item->sha256_hash = hash;
  item->size = size;
  item->frame_url = file_writer->context().url;
  item->has_user_gesture = file_writer->has_transient_user_activation_;
  file_writer->manager()->permission_context()->PerformAfterWriteChecks(
      std::move(item), file_writer->context().frame_id,
      base::BindOnce(&NativeFileSystemFileWriterImpl::DidAfterWriteCheck,
                     file_writer, std::move(manager), swap_url,
                     std::move(callback)));
}

// static
void NativeFileSystemFileWriterImpl::DidAfterWriteCheck(
    base::WeakPtr<NativeFileSystemFileWriterImpl> file_writer,
    scoped_refptr<NativeFileSystemManagerImpl> manager,
    const storage::FileSystemURL& swap_url,
    NativeFileSystemFileWriterImpl::CloseCallback callback,
    NativeFileSystemPermissionContext::AfterWriteCheckResult result) {
  if (file_writer &&
      result ==
          NativeFileSystemPermissionContext::AfterWriteCheckResult::kAllow) {
    file_writer->DidPassAfterWriteCheck(std::move(callback));
    return;
  }

  // Writer is gone, or safe browsing check failed. In this case we should
  // try deleting the swap file and call the callback to report that close
  // failed.
  manager->operation_runner().PostTaskWithThisObject(
      FROM_HERE, base::BindOnce(&RemoveSwapFile, swap_url));
  std::move(callback).Run(native_file_system_error::FromStatus(
      NativeFileSystemStatus::kOperationAborted,
      "Write operation blocked by Safe Browsing."));
  return;
}

void NativeFileSystemFileWriterImpl::DidPassAfterWriteCheck(
    CloseCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
        base::BindOnce(&NativeFileSystemFileWriterImpl::DidSwapFileDoQuarantine,
                       weak_factory_.GetWeakPtr(), url(), referrer_url,
                       std::move(quarantine_remote), std::move(callback));
  } else {
    result_callback = base::BindOnce(
        &NativeFileSystemFileWriterImpl::DidSwapFileSkipQuarantine,
        weak_factory_.GetWeakPtr(), std::move(callback));
  }
  DoFileSystemOperation(
      FROM_HERE, &FileSystemOperationRunner::MoveFileLocal,
      std::move(result_callback), swap_url(), url(),
      storage::FileSystemOperation::OPTION_PRESERVE_LAST_MODIFIED);
}

void NativeFileSystemFileWriterImpl::DidSwapFileSkipQuarantine(
    CloseCallback callback,
    base::File::Error result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (result != base::File::FILE_OK) {
    state_ = State::kCloseError;
    DLOG(ERROR) << "Swap file move operation failed source: "
                << swap_url().path() << " dest: " << url().path()
                << " error: " << base::File::ErrorToString(result);
    std::move(callback).Run(native_file_system_error::FromFileError(result));
    return;
  }

  state_ = State::kClosed;
  std::move(callback).Run(native_file_system_error::Ok());
}

// static
void NativeFileSystemFileWriterImpl::DidSwapFileDoQuarantine(
    base::WeakPtr<NativeFileSystemFileWriterImpl> file_writer,
    const storage::FileSystemURL& target_url,
    const GURL& referrer_url,
    mojo::Remote<quarantine::mojom::Quarantine> quarantine_remote,
    CloseCallback callback,
    base::File::Error result) {
  if (file_writer)
    DCHECK_CALLED_ON_VALID_SEQUENCE(file_writer->sequence_checker_);

  if (result != base::File::FILE_OK) {
    if (file_writer)
      file_writer->state_ = State::kCloseError;
    DLOG(ERROR) << "Swap file move operation failed dest: " << target_url.path()
                << " error: " << base::File::ErrorToString(result);
    std::move(callback).Run(native_file_system_error::FromFileError(result));
    return;
  }

  // The quarantine service operates on files identified by a base::FilePath. As
  // such we can only quarantine files that are actual local files.
  // On ChromeOS on the other hand anything that isn't in the sandboxed file
  // system is also uniquely identifiable by its FileSystemURL::path(), and
  // thus we accept all other FileSystemURL types.
#if defined(OS_CHROMEOS)
  DCHECK(target_url.type() != storage::kFileSystemTypeTemporary &&
         target_url.type() != storage::kFileSystemTypePersistent)
      << target_url.type();
#else
  DCHECK(target_url.type() == storage::kFileSystemTypeNativeLocal ||
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
            base::BindOnce(&NativeFileSystemFileWriterImpl::DidAnnotateFile,
                           std::move(file_writer), std::move(callback),
                           std::move(quarantine_remote)),
            quarantine::mojom::QuarantineFileResult::ANNOTATION_FAILED));
  } else {
#if defined(OS_WIN)
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&quarantine::SetInternetZoneIdentifierDirectly,
                       target_url.path(), authority_url, referrer_url),
        base::BindOnce(&NativeFileSystemFileWriterImpl::DidAnnotateFile,
                       std::move(file_writer), std::move(callback),
                       std::move(quarantine_remote)));
#else
    if (file_writer) {
      file_writer->DidAnnotateFile(
          std::move(callback), std::move(quarantine_remote),
          quarantine::mojom::QuarantineFileResult::ANNOTATION_FAILED);
    }
#endif
  }
}

void NativeFileSystemFileWriterImpl::DidAnnotateFile(
    CloseCallback callback,
    mojo::Remote<quarantine::mojom::Quarantine> quarantine_remote,
    quarantine::mojom::QuarantineFileResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  state_ = State::kClosed;

  if (result != quarantine::mojom::QuarantineFileResult::OK &&
      result != quarantine::mojom::QuarantineFileResult::ANNOTATION_FAILED) {
    // If malware was detected, or the file referrer was blocked by policy, the
    // file will be deleted at this point by AttachmentServices on Windows.
    // There is nothing to do except to return the error message to the
    // application.
    std::move(callback).Run(native_file_system_error::FromStatus(
        NativeFileSystemStatus::kOperationAborted,
        "Write operation aborted due to security policy."));
    return;
  }

  std::move(callback).Run(native_file_system_error::Ok());
}

void NativeFileSystemFileWriterImpl::ComputeHashForSwapFile(
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

base::WeakPtr<NativeFileSystemHandleBase>
NativeFileSystemFileWriterImpl::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace content

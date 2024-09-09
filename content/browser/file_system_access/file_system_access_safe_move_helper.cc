// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_safe_move_helper.h"

#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/thread_annotations.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/services/quarantine/quarantine.h"
#include "content/browser/file_system_access/features.h"
#include "content/browser/file_system_access/file_system_access_error.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_client.h"
#include "crypto/secure_hash.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "storage/browser/file_system/copy_or_move_hook_delegate.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "storage/browser/file_system/file_system_backend.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/common/file_system/file_system_util.h"

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
      FileSystemAccessSafeMoveHelper::HashCallback callback,
      const storage::FileSystemURL& source_url,
      storage::FileSystemOperationRunner*) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    auto calculator = base::MakeRefCounted<HashCalculator>(std::move(context),
                                                           std::move(callback));
    calculator->Start(source_url);
  }

  HashCalculator(scoped_refptr<storage::FileSystemContext> context,
                 FileSystemAccessSafeMoveHelper::HashCallback callback)
      : context_(std::move(context)), callback_(std::move(callback)) {
    DCHECK(context_);
  }

 private:
  friend class base::RefCounted<HashCalculator>;
  ~HashCalculator() = default;

  void Start(const storage::FileSystemURL& source_url) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    reader_ = context_->CreateFileStreamReader(
        source_url, 0, storage::kMaximumLength, base::Time());
    int64_t length =
        reader_->GetLength(base::BindOnce(&HashCalculator::GotLength, this));
    if (length == net::ERR_IO_PENDING)
      return;
    GotLength(length);
  }

  void GotLength(int64_t length) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (length < 0) {
      std::move(callback_).Run(storage::NetErrorToFileError(length),
                               std::string(), -1);
      return;
    }

    file_size_ = length;
    ReadMore();
  }

  void ReadMore() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK_GE(file_size_, 0);
    int read_result =
        reader_->Read(buffer_.get(), buffer_->size(),
                      base::BindOnce(&HashCalculator::DidRead, this));
    if (read_result == net::ERR_IO_PENDING)
      return;
    DidRead(read_result);
  }

  void DidRead(int bytes_read) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK_GE(file_size_, 0);
    if (bytes_read < 0) {
      std::move(callback_).Run(storage::NetErrorToFileError(bytes_read),
                               std::string(), -1);
      return;
    }
    if (bytes_read == 0) {
      std::string hash_str(hash_->GetHashLength(), 0);
      hash_->Finish(std::data(hash_str), hash_str.size());
      std::move(callback_).Run(base::File::FILE_OK, hash_str, file_size_);
      return;
    }

    hash_->Update(buffer_->data(), bytes_read);
    ReadMore();
  }

  SEQUENCE_CHECKER(sequence_checker_);

  const scoped_refptr<storage::FileSystemContext> context_;
  FileSystemAccessSafeMoveHelper::HashCallback callback_
      GUARDED_BY_CONTEXT(sequence_checker_);

  const scoped_refptr<net::IOBufferWithSize> buffer_{
      base::MakeRefCounted<net::IOBufferWithSize>(8 * 1024)};

  const std::unique_ptr<crypto::SecureHash> hash_{
      crypto::SecureHash::Create(crypto::SecureHash::SHA256)};

  std::unique_ptr<storage::FileStreamReader> reader_
      GUARDED_BY_CONTEXT(sequence_checker_);
  int64_t file_size_ GUARDED_BY_CONTEXT(sequence_checker_) = -1;
};

}  // namespace

FileSystemAccessSafeMoveHelper::FileSystemAccessSafeMoveHelper(
    base::WeakPtr<FileSystemAccessManagerImpl> manager,
    const FileSystemAccessManagerImpl::BindingContext& context,
    const storage::FileSystemURL& source_url,
    const storage::FileSystemURL& dest_url,
    storage::FileSystemOperation::CopyOrMoveOptionSet options,
    download::QuarantineConnectionCallback quarantine_connection_callback,
    bool has_transient_user_activation)
    : manager_(std::move(manager)),
      context_(context),
      source_url_(source_url),
      dest_url_(dest_url),
      options_(options),
      quarantine_connection_callback_(
          std::move(quarantine_connection_callback)),
      has_transient_user_activation_(has_transient_user_activation) {}

FileSystemAccessSafeMoveHelper::~FileSystemAccessSafeMoveHelper() = default;

void FileSystemAccessSafeMoveHelper::Start(
    FileSystemAccessSafeMoveHelperCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  callback_ = std::move(callback);

  if (!manager_) {
    std::move(callback_).Run(file_system_access_error::FromStatus(
        blink::mojom::FileSystemAccessStatus::kOperationAborted));
    return;
  }

  if (!RequireAfterWriteChecks() || !manager_->permission_context()) {
    DidAfterWriteCheck(
        FileSystemAccessPermissionContext::AfterWriteCheckResult::kAllow);
    return;
  }

  ComputeHashForSourceFile(
      base::BindOnce(&FileSystemAccessSafeMoveHelper::DoAfterWriteCheck,
                     weak_factory_.GetWeakPtr()));
}

void FileSystemAccessSafeMoveHelper::ComputeHashForSourceFile(
    HashCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!manager_) {
    std::move(callback_).Run(file_system_access_error::FromStatus(
        blink::mojom::FileSystemAccessStatus::kOperationAborted));
    return;
  }

  auto wrapped_callback =
      base::BindPostTaskToCurrentDefault(std::move(callback));
  manager_->operation_runner().PostTaskWithThisObject(
      base::BindOnce(&HashCalculator::CreateAndStart,
                     base::WrapRefCounted(manager_->context()),
                     std::move(wrapped_callback), source_url()));
}

bool FileSystemAccessSafeMoveHelper::RequireAfterWriteChecks() const {
  if (dest_url().type() == storage::kFileSystemTypeTemporary)
    return false;

  if (!source_url().IsInSameFileSystem(dest_url()))
    return true;

  // TODO(crbug.com/40198034): Properly handle directory moves here, for
  // which extension checks don't make sense.
  auto source_extension = source_url().path().Extension();
  auto dest_extension = dest_url().path().Extension();
  return source_extension.empty() || source_extension != dest_extension;
}

bool FileSystemAccessSafeMoveHelper::RequireQuarantine() const {
  return dest_url().type() != storage::kFileSystemTypeTemporary;
}

void FileSystemAccessSafeMoveHelper::DoAfterWriteCheck(
    base::File::Error hash_result,
    const std::string& hash,
    int64_t size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (hash_result != base::File::FILE_OK) {
    // Calculating the hash failed.
    std::move(callback_).Run(file_system_access_error::FromStatus(
        blink::mojom::FileSystemAccessStatus::kOperationAborted,
        "Failed to perform Safe Browsing check."));
    return;
  }

  if (!manager_) {
    std::move(callback_).Run(file_system_access_error::FromStatus(
        blink::mojom::FileSystemAccessStatus::kOperationAborted));
    return;
  }

  content::GlobalRenderFrameHostId outermost_main_frame_id;
  auto* rfh = content::RenderFrameHost::FromID(context_.frame_id);
  if (rfh)
    outermost_main_frame_id = rfh->GetOutermostMainFrame()->GetGlobalId();

  auto item = std::make_unique<FileSystemAccessWriteItem>();
  item->target_file_path = dest_url().path();
  item->full_path = source_url().path();
  item->sha256_hash = hash;
  item->size = size;
  item->frame_url = context_.url;
  item->outermost_main_frame_id = outermost_main_frame_id;
  item->has_user_gesture = has_transient_user_activation_;
  manager_->permission_context()->PerformAfterWriteChecks(
      std::move(item), context_.frame_id,
      base::BindOnce(&FileSystemAccessSafeMoveHelper::DidAfterWriteCheck,
                     weak_factory_.GetWeakPtr()));
}

void FileSystemAccessSafeMoveHelper::DidAfterWriteCheck(
    FileSystemAccessPermissionContext::AfterWriteCheckResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (result !=
      FileSystemAccessPermissionContext::AfterWriteCheckResult::kAllow) {
    // Safe browsing check failed.
    std::move(callback_).Run(file_system_access_error::FromStatus(
        blink::mojom::FileSystemAccessStatus::kOperationAborted,
        "Blocked by Safe Browsing."));
    return;
  }

  if (!manager_) {
    std::move(callback_).Run(file_system_access_error::FromStatus(
        blink::mojom::FileSystemAccessStatus::kOperationAborted));
    return;
  }

  // If the move operation succeeds, the path pointing to the source file will
  // not exist anymore. In case of error, the source file URL will point to a
  // valid filesystem location.
  base::OnceCallback<void(base::File::Error)> result_callback;
  if (RequireQuarantine()) {
    GURL referrer_url = manager_->is_off_the_record() ? GURL() : context_.url;
    mojo::Remote<quarantine::mojom::Quarantine> quarantine_remote;
    if (quarantine_connection_callback_) {
      quarantine_connection_callback_.Run(
          quarantine_remote.BindNewPipeAndPassReceiver());
    }
    result_callback =
        base::BindOnce(&FileSystemAccessSafeMoveHelper::DidFileDoQuarantine,
                       weak_factory_.GetWeakPtr(), dest_url(), referrer_url,
                       std::move(quarantine_remote));
  } else {
    result_callback =
        base::BindOnce(&FileSystemAccessSafeMoveHelper::DidFileSkipQuarantine,
                       weak_factory_.GetWeakPtr());
  }
  manager_->DoFileSystemOperation(
      FROM_HERE, &storage::FileSystemOperationRunner::Move,
      std::move(result_callback), source_url(), dest_url(), options_,
      storage::FileSystemOperationRunner::ErrorBehavior::ERROR_BEHAVIOR_ABORT,
      std::make_unique<storage::CopyOrMoveHookDelegate>());
}

void FileSystemAccessSafeMoveHelper::DidFileSkipQuarantine(
    base::File::Error result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback_).Run(file_system_access_error::FromFileError(result));
}

void FileSystemAccessSafeMoveHelper::DidFileDoQuarantine(
    const storage::FileSystemURL& target_url,
    const GURL& referrer_url,
    mojo::Remote<quarantine::mojom::Quarantine> quarantine_remote,
    base::File::Error result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (result != base::File::FILE_OK) {
    DLOG(ERROR) << "Move operation failed source: " << source_url().path()
                << " dest: " << target_url.path()
                << " error: " << base::File::ErrorToString(result);
    std::move(callback_).Run(file_system_access_error::FromFileError(result));
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
        // TODO(crbug.com/351165321): Consider propagating request_initiator
        // information here.
        /*request_initiator=*/std::nullopt,
        GetContentClient()
            ->browser()
            ->GetApplicationClientGUIDForQuarantineCheck(),
        mojo::WrapCallbackWithDefaultInvokeIfNotRun(
            base::BindOnce(&FileSystemAccessSafeMoveHelper::DidAnnotateFile,
                           weak_factory_.GetWeakPtr(),
                           std::move(quarantine_remote)),
            quarantine::mojom::QuarantineFileResult::ANNOTATION_FAILED));
  } else {
#if BUILDFLAG(IS_WIN)
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&quarantine::SetInternetZoneIdentifierDirectly,
                       target_url.path(), authority_url, referrer_url),
        base::BindOnce(&FileSystemAccessSafeMoveHelper::DidAnnotateFile,
                       weak_factory_.GetWeakPtr(),
                       std::move(quarantine_remote)));
#else
    DidAnnotateFile(std::move(quarantine_remote),
                    quarantine::mojom::QuarantineFileResult::ANNOTATION_FAILED);
#endif
  }
}

void FileSystemAccessSafeMoveHelper::DidAnnotateFile(
    mojo::Remote<quarantine::mojom::Quarantine> quarantine_remote,
    quarantine::mojom::QuarantineFileResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (result != quarantine::mojom::QuarantineFileResult::OK &&
      result != quarantine::mojom::QuarantineFileResult::ANNOTATION_FAILED) {
    // If malware was detected, or the file referrer was blocked by policy, the
    // file will be deleted at this point by AttachmentServices on Windows.
    // There is nothing to do except to return the error message to the
    // application.
    std::move(callback_).Run(file_system_access_error::FromStatus(
        blink::mojom::FileSystemAccessStatus::kOperationAborted,
        "Aborted due to security policy."));
    return;
  }

  std::move(callback_).Run(file_system_access_error::Ok());
}

}  // namespace content

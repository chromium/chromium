// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system/file_system_manager_impl.h"

#include <utility>

#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "components/services/filesystem/public/mojom/types.mojom.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/file_system/browser_file_system_helper.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_impl.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/blob/shareable_file_reference.h"
#include "storage/browser/file_system/copy_or_move_hook_delegate.h"
#include "storage/browser/file_system/file_observers.h"
#include "storage/browser/file_system/file_permission_policy.h"
#include "storage/browser/file_system/file_system_backend.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_file_util.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/isolated_context.h"
#include "storage/common/file_system/file_system_info.h"
#include "storage/common/file_system/file_system_types.h"
#include "storage/common/file_system/file_system_util.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "third_party/blink/public/mojom/blob/serialized_blob.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

using storage::BlobDataBuilder;
using storage::BlobStorageContext;
using storage::FileSystemBackend;
using storage::FileSystemFileUtil;
using storage::FileSystemOperation;
using storage::FileSystemURL;

namespace content {

namespace {

void RevokeFilePermission(int child_id, const base::FilePath& path) {
  ChildProcessSecurityPolicyImpl::GetInstance()->RevokeAllPermissionsForFile(
      child_id, path);
}

storage::FileSystemType ToStorageFileSystemType(
    blink::mojom::FileSystemType type) {
  switch (type) {
    case blink::mojom::FileSystemType::kTemporary:
      return storage::FileSystemType::kFileSystemTypeTemporary;
    case blink::mojom::FileSystemType::kPersistent:
      return storage::FileSystemType::kFileSystemTypePersistent;
    case blink::mojom::FileSystemType::kIsolated:
      return storage::FileSystemType::kFileSystemTypeIsolated;
    case blink::mojom::FileSystemType::kExternal:
      return storage::FileSystemType::kFileSystemTypeExternal;
  }
  NOTREACHED_IN_MIGRATION();
  return storage::FileSystemType::kFileSystemTypeTemporary;
}

blink::mojom::FileSystemType ToMojoFileSystemType(
    storage::FileSystemType type) {
  switch (type) {
    case storage::FileSystemType::kFileSystemTypeTemporary:
      return blink::mojom::FileSystemType::kTemporary;
    case storage::FileSystemType::kFileSystemTypePersistent:
      return blink::mojom::FileSystemType::kPersistent;
    case storage::FileSystemType::kFileSystemTypeIsolated:
      return blink::mojom::FileSystemType::kIsolated;
    case storage::FileSystemType::kFileSystemTypeExternal:
      return blink::mojom::FileSystemType::kExternal;
    // Internal enum types
    case storage::FileSystemType::kFileSystemTypeUnknown:
    case storage::FileSystemType::kFileSystemInternalTypeEnumStart:
    case storage::FileSystemType::kFileSystemTypeTest:
    case storage::FileSystemType::kFileSystemTypeLocal:
    case storage::FileSystemType::kFileSystemTypeDragged:
    case storage::FileSystemType::kFileSystemTypeLocalMedia:
    case storage::FileSystemType::kFileSystemTypeDeviceMedia:
    case storage::FileSystemType::kFileSystemTypeSyncable:
    case storage::FileSystemType::kFileSystemTypeSyncableForInternalSync:
    case storage::FileSystemType::kFileSystemTypeLocalForPlatformApp:
    case storage::FileSystemType::kFileSystemTypeForTransientFile:
    case storage::FileSystemType::kFileSystemTypeProvided:
    case storage::FileSystemType::kFileSystemTypeDeviceMediaAsFileStorage:
    case storage::FileSystemType::kFileSystemTypeArcContent:
    case storage::FileSystemType::kFileSystemTypeArcDocumentsProvider:
    case storage::FileSystemType::kFileSystemTypeDriveFs:
    case storage::FileSystemType::kFileSystemTypeSmbFs:
    case storage::FileSystemType::kFileSystemTypeFuseBox:
    case storage::FileSystemType::kFileSystemInternalTypeEnumEnd:
      NOTREACHED_IN_MIGRATION();
      return blink::mojom::FileSystemType::kTemporary;
  }
  NOTREACHED_IN_MIGRATION();
  return blink::mojom::FileSystemType::kTemporary;
}

blink::mojom::FileSystemInfoPtr ToMojoFileSystemInfo(
    const storage::FileSystemInfo& info) {
  return blink::mojom::FileSystemInfo::New(
      info.name, info.root_url, ToMojoFileSystemType(info.mount_type));
}

}  // namespace

class FileSystemManagerImpl::FileSystemCancellableOperationImpl
    : public blink::mojom::FileSystemCancellableOperation {
  using OperationID = storage::FileSystemOperationRunner::OperationID;

 public:
  FileSystemCancellableOperationImpl(
      OperationID id,
      FileSystemManagerImpl* file_system_manager_impl)
      : id_(id), file_system_manager_impl_(file_system_manager_impl) {}
  ~FileSystemCancellableOperationImpl() override = default;

 private:
  void Cancel(CancelCallback callback) override {
    file_system_manager_impl_->Cancel(id_, std::move(callback));
  }

  const OperationID id_;
  // |file_system_manager_impl| owns |this| through a UniqueReceiverSet.
  const raw_ptr<FileSystemManagerImpl> file_system_manager_impl_;
};

class FileSystemManagerImpl::ReceivedSnapshotListenerImpl
    : public blink::mojom::ReceivedSnapshotListener {
 public:
  ReceivedSnapshotListenerImpl(int snapshot_id,
                               FileSystemManagerImpl* file_system_manager_impl)
      : snapshot_id_(snapshot_id),
        file_system_manager_impl_(file_system_manager_impl) {}
  ~ReceivedSnapshotListenerImpl() override = default;

 private:
  void DidReceiveSnapshotFile() override {
    file_system_manager_impl_->DidReceiveSnapshotFile(snapshot_id_);
  }

  const int snapshot_id_;
  // |file_system_manager_impl| owns |this| through a UniqueReceiverSet.
  const raw_ptr<FileSystemManagerImpl> file_system_manager_impl_;
};

struct FileSystemManagerImpl::WriteSyncCallbackEntry {
  WriteSyncCallback callback;
  int64_t bytes;

  explicit WriteSyncCallbackEntry(WriteSyncCallback cb)
      : callback(std::move(cb)), bytes(0) {}
};

struct FileSystemManagerImpl::ReadDirectorySyncCallbackEntry {
  ReadDirectorySyncCallback callback;
  std::vector<filesystem::mojom::DirectoryEntryPtr> entries;

  explicit ReadDirectorySyncCallbackEntry(ReadDirectorySyncCallback cb)
      : callback(std::move(cb)) {}
};

FileSystemManagerImpl::FileSystemManagerImpl(
    int process_id,
    scoped_refptr<storage::FileSystemContext> file_system_context,
    scoped_refptr<ChromeBlobStorageContext> blob_storage_context)
    : process_id_(process_id),
      context_(std::move(file_system_context)),
      blob_storage_context_(std::move(blob_storage_context)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(context_);
  DCHECK(blob_storage_context_);
  receivers_.set_disconnect_handler(base::BindRepeating(
      &FileSystemManagerImpl::OnConnectionError, base::Unretained(this)));
}

FileSystemManagerImpl::~FileSystemManagerImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

base::WeakPtr<FileSystemManagerImpl> FileSystemManagerImpl::GetWeakPtr() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return weak_factory_.GetWeakPtr();
}

void FileSystemManagerImpl::BindReceiver(
    const blink::StorageKey& storage_key,
    mojo::PendingReceiver<blink::mojom::FileSystemManager> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!operation_runner_)
    operation_runner_ = context_->CreateFileSystemOperationRunner();
  receivers_.Add(this, std::move(receiver), storage_key);
}

void FileSystemManagerImpl::Open(const url::Origin& origin,
                                 blink::mojom::FileSystemType file_system_type,
                                 OpenCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  GetUIThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &ChildProcessSecurityPolicyImpl::CanAccessDataForOrigin,
          base::Unretained(ChildProcessSecurityPolicyImpl::GetInstance()),
          process_id_, origin),
      base::BindOnce(&FileSystemManagerImpl::ContinueOpen,
                     weak_factory_.GetWeakPtr(), origin, file_system_type,
                     receivers_.GetBadMessageCallback(), std::move(callback),
                     receivers_.current_context()));
}

void FileSystemManagerImpl::ContinueOpen(
    const url::Origin& origin,
    blink::mojom::FileSystemType file_system_type,
    mojo::ReportBadMessageCallback bad_message_callback,
    OpenCallback callback,
    const blink::StorageKey& storage_key,
    bool security_check_success) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!security_check_success) {
    std::move(bad_message_callback).Run("FSMI_OPEN_INVALID_ORIGIN");
    return;
  }

  context_->OpenFileSystem(
      storage_key, /*bucket=*/std::nullopt,
      ToStorageFileSystemType(file_system_type),
      storage::OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
      base::BindOnce(&FileSystemManagerImpl::DidOpenFileSystem, GetWeakPtr(),
                     std::move(callback)));
}

void FileSystemManagerImpl::ResolveURL(const GURL& filesystem_url,
                                       ResolveURLCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  FileSystemURL url(
      context_->CrackURL(filesystem_url, receivers_.current_context()));
  std::optional<base::File::Error> opt_error = ValidateFileSystemURL(url);
  if (opt_error) {
    std::move(callback).Run(blink::mojom::FileSystemInfo::New(),
                            base::FilePath(), false, opt_error.value());
    return;
  }

  GetUIThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &ChildProcessSecurityPolicyImpl::CanReadFileSystemFile,
          base::Unretained(ChildProcessSecurityPolicyImpl::GetInstance()),
          process_id_, url),
      base::BindOnce(&FileSystemManagerImpl::ContinueResolveURL,
                     weak_factory_.GetWeakPtr(), url, std::move(callback)));
}

void FileSystemManagerImpl::ContinueResolveURL(
    const storage::FileSystemURL& url,
    ResolveURLCallback callback,
    bool security_check_success) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!security_check_success) {
    std::move(callback).Run(blink::mojom::FileSystemInfo::New(),
                            base::FilePath(), false,
                            base::File::FILE_ERROR_SECURITY);
    return;
  }

  context_->ResolveURL(
      url, base::BindOnce(&FileSystemManagerImpl::DidResolveURL, GetWeakPtr(),
                          std::move(callback)));
}

void FileSystemManagerImpl::Move(const GURL& src_path,
                                 const GURL& dest_path,
                                 MoveCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  FileSystemURL src_url(
      context_->CrackURL(src_path, receivers_.current_context()));
  FileSystemURL dest_url(
      context_->CrackURL(dest_path, receivers_.current_context()));
  std::optional<base::File::Error> opt_error = ValidateFileSystemURL(src_url);
  if (!opt_error)
    opt_error = ValidateFileSystemURL(dest_url);
  if (opt_error) {
    std::move(callback).Run(opt_error.value());
    return;
  }
  GetUIThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &ChildProcessSecurityPolicyImpl::CanMoveFileSystemFile,
          base::Unretained(ChildProcessSecurityPolicyImpl::GetInstance()),
          process_id_, src_url, dest_url),
      base::BindOnce(&FileSystemManagerImpl::ContinueMove,
                     weak_factory_.GetWeakPtr(), src_url, dest_url,
                     std::move(callback)));
}

void FileSystemManagerImpl::ContinueMove(const storage::FileSystemURL& src_url,
                                         const storage::FileSystemURL& dest_url,
                                         MoveCallback callback,
                                         bool security_check_success) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!security_check_success) {
    std::move(callback).Run(base::File::FILE_ERROR_SECURITY);
    return;
  }

  storage::FileSystemOperationRunner* fs_op_runner = operation_runner();
  if (!fs_op_runner) {
    /* A null FileSystemOperationRunner at this point means the corresponding
     * renderer was terminated, so return early to ignore the requested
     * FileSystemOperation. */
    return;
  }

  fs_op_runner->Move(src_url, dest_url,
                     storage::FileSystemOperation::CopyOrMoveOptionSet(),
                     FileSystemOperation::ERROR_BEHAVIOR_ABORT,
                     std::make_unique<storage::CopyOrMoveHookDelegate>(),
                     base::BindOnce(&FileSystemManagerImpl::DidFinish,
                                    GetWeakPtr(), std::move(callback)));
}

void FileSystemManagerImpl::Copy(const GURL& src_path,
                                 const GURL& dest_path,
                                 CopyCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  FileSystemURL src_url(
      context_->CrackURL(src_path, receivers_.current_context()));
  FileSystemURL dest_url(
      context_->CrackURL(dest_path, receivers_.current_context()));
  std::optional<base::File::Error> opt_error = ValidateFileSystemURL(src_url);
  if (!opt_error)
    opt_error = ValidateFileSystemURL(dest_url);
  if (opt_error) {
    std::move(callback).Run(opt_error.value());
    return;
  }

  GetUIThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &ChildProcessSecurityPolicyImpl::CanCopyFileSystemFile,
          base::Unretained(ChildProcessSecurityPolicyImpl::GetInstance()),
          process_id_, src_url, dest_url),
      base::BindOnce(&FileSystemManagerImpl::ContinueCopy,
                     weak_factory_.GetWeakPtr(), src_url, dest_url,
                     std::move(callback)));
}

void FileSystemManagerImpl::ContinueCopy(const storage::FileSystemURL& src_url,
                                         const storage::FileSystemURL& dest_url,
                                         CopyCallback callback,
                                         bool security_check_success) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!security_check_success) {
    std::move(callback).Run(base::File::FILE_ERROR_SECURITY);
    return;
  }

  storage::FileSystemOperationRunner* fs_op_runner = operation_runner();
  if (!fs_op_runner) {
    /* A null FileSystemOperationRunner at this point means the corresponding
     * renderer was terminated, so return early to ignore the requested
     * FileSystemOperation. */
    return;
  }

  fs_op_runner->Copy(src_url, dest_url,
                     storage::FileSystemOperation::CopyOrMoveOptionSet(),
                     FileSystemOperation::ERROR_BEHAVIOR_ABORT,
                     std::make_unique<storage::CopyOrMoveHookDelegate>(),
                     base::BindOnce(&FileSystemManagerImpl::DidFinish,
                                    GetWeakPtr(), std::move(callback)));
}

void FileSystemManagerImpl::Remove(const GURL& path,
                                   bool recursive,
                                   RemoveCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  FileSystemURL url(context_->CrackURL(path, receivers_.current_context()));
  std::optional<base::File::Error> opt_error = ValidateFileSystemURL(url);
  if (opt_error) {
    std::move(callback).Run(opt_error.value());
    return;
  }

  GetUIThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &ChildProcessSecurityPolicyImpl::CanDeleteFileSystemFile,
          base::Unretained(ChildProcessSecurityPolicyImpl::GetInstance()),
          process_id_, url),
      base::BindOnce(&FileSystemManagerImpl::ContinueRemove,
                     weak_factory_.GetWeakPtr(), url, recursive,
                     std::move(callback)));
}

void FileSystemManagerImpl::ContinueRemove(const storage::FileSystemURL& url,
                                           bool recursive,
                                           RemoveCallback callback,
                                           bool security_check_success) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!security_check_success) {
    std::move(callback).Run(base::File::FILE_ERROR_SECURITY);
    return;
  }

  storage::FileSystemOperationRunner* fs_op_runner = operation_runner();
  if (!fs_op_runner) {
    /* A null FileSystemOperationRunner at this point means the corresponding
     * renderer was terminated, so return early to ignore the requested
     * FileSystemOperation. */
    return;
  }

  fs_op_runner->Remove(url, recursive,
                       base::BindOnce(&FileSystemManagerImpl::DidFinish,
                                      GetWeakPtr(), std::move(callback)));
}

void FileSystemManagerImpl::ReadMetadata(const GURL& path,
                                         ReadMetadataCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  FileSystemURL url(context_->CrackURL(path, receivers_.current_context()));
  std::optional<base::File::Error> opt_error = ValidateFileSystemURL(url);
  if (opt_error) {
    std::move(callback).Run(base::File::Info(), opt_error.value());
    return;
  }

  GetUIThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &ChildProcessSecurityPolicyImpl::CanReadFileSystemFile,
          base::Unretained(ChildProcessSecurityPolicyImpl::GetInstance()),
          process_id_, url),
      base::BindOnce(&FileSystemManagerImpl::ContinueReadMetadata,
                     weak_factory_.GetWeakPtr(), url, std::move(callback)));
}

void FileSystemManagerImpl::ContinueReadMetadata(
    const storage::FileSystemURL& url,
    ReadMetadataCallback callback,
    bool security_check_success) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!security_check_success) {
    std::move(callback).Run(base::File::Info(),
                            base::File::FILE_ERROR_SECURITY);
    return;
  }

  storage::FileSystemOperationRunner* fs_op_runner = operation_runner();
  if (!fs_op_runner) {
    /* A null FileSystemOperationRunner at this point means the corresponding
     * renderer was terminated, so return early to ignore the requested
     * FileSystemOperation. */
    return;
  }

  fs_op_runner->GetMetadata(
      url,
      {storage::FileSystemOperation::GetMetadataField::kIsDirectory,
       storage::FileSystemOperation::GetMetadataField::kSize,
       storage::FileSystemOperation::GetMetadataField::kLastModified},
      base::BindOnce(&FileSystemManagerImpl::DidGetMetadata, GetWeakPtr(),
                     std::move(callback)));
}

void FileSystemManagerImpl::Create(const GURL& path,
                                   bool exclusive,
                                   bool is_directory,
                                   bool recursive,
                                   CreateCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  FileSystemURL url(context_->CrackURL(path, receivers_.current_context()));
  std::optional<base::File::Error> opt_error = ValidateFileSystemURL(url);
  if (opt_error) {
    std::move(callback).Run(opt_error.value());
    return;
  }

  GetUIThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &ChildProcessSecurityPolicyImpl::CanCreateFileSystemFile,
          base::Unretained(ChildProcessSecurityPolicyImpl::GetInstance()),
          process_id_, url),
      base::BindOnce(&FileSystemManagerImpl::ContinueCreate,
                     weak_factory_.GetWeakPtr(), url, exclusive, is_directory,
                     recursive, std::move(callback)));
}

void FileSystemManagerImpl::ContinueCreate(const storage::FileSystemURL& url,
                                           bool exclusive,
                                           bool is_directory,
                                           bool recursive,
                                           CreateCallback callback,
                                           bool security_check_success) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!security_check_success) {
    std::move(callback).Run(base::File::FILE_ERROR_SECURITY);
    return;
  }

  storage::FileSystemOperationRunner* fs_op_runner = operation_runner();
  if (!fs_op_runner) {
    /* A null FileSystemOperationRunner at this point means the corresponding
     * renderer was terminated, so return early to ignore the requested
     * FileSystemOperation. */
    return;
  }

  if (is_directory) {
    fs_op_runner->CreateDirectory(
        url, exclusive, recursive,
        base::BindOnce(&FileSystemManagerImpl::DidFinish, GetWeakPtr(),
                       std::move(callback)));
  } else {
    fs_op_runner->CreateFile(url, exclusive,
                             base::BindOnce(&FileSystemManagerImpl::DidFinish,
                                            GetWeakPtr(), std::move(callback)));
  }
}

void FileSystemManagerImpl::Exists(const GURL& path,
                                   bool is_directory,
                                   ExistsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  FileSystemURL url(context_->CrackURL(path, receivers_.current_context()));
  std::optional<base::File::Error> opt_error = ValidateFileSystemURL(url);
  if (opt_error) {
    std::move(callback).Run(opt_error.value());
    return;
  }

  GetUIThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &ChildProcessSecurityPolicyImpl::CanReadFileSystemFile,
          base::Unretained(ChildProcessSecurityPolicyImpl::GetInstance()),
          process_id_, url),
      base::BindOnce(&FileSystemManagerImpl::ContinueExists,
                     weak_factory_.GetWeakPtr(), url, is_directory,
                     std::move(callback)));
}

void FileSystemManagerImpl::ContinueExists(const storage::FileSystemURL& url,
                                           bool is_directory,
                                           ExistsCallback callback,
                                           bool security_check_success) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!security_check_success) {
    std::move(callback).Run(base::File::FILE_ERROR_SECURITY);
    return;
  }

  storage::FileSystemOperationRunner* fs_op_runner = operation_runner();
  if (!fs_op_runner) {
    /* A null FileSystemOperationRunner at this point means the corresponding
     * renderer was terminated, so return early to ignore the requested
     * FileSystemOperation. */
    return;
  }

  if (is_directory) {
    fs_op_runner->DirectoryExists(
        url, base::BindOnce(&FileSystemManagerImpl::DidFinish, GetWeakPtr(),
                            std::move(callback)));
  } else {
    fs_op_runner->FileExists(
        url, base::BindOnce(&FileSystemManagerImpl::DidFinish, GetWeakPtr(),
                            std::move(callback)));
  }
}

void FileSystemManagerImpl::ReadDirectory(
    const GURL& path,
    mojo::PendingRemote<blink::mojom::FileSystemOperationListener>
        pending_listener) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  FileSystemURL url(context_->CrackURL(path, receivers_.current_context()));
  std::optional<base::File::Error> opt_error = ValidateFileSystemURL(url);
  mojo::Remote<blink::mojom::FileSystemOperationListener> listener(
      std::move(pending_listener));
  if (opt_error) {
    listener->ErrorOccurred(opt_error.value());
    return;
  }

  GetUIThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &ChildProcessSecurityPolicyImpl::CanReadFileSystemFile,
          base::Unretained(ChildProcessSecurityPolicyImpl::GetInstance()),
          process_id_, url),
      base::BindOnce(&FileSystemManagerImpl::ContinueReadDirectory,
                     weak_factory_.GetWeakPtr(), url, std::move(listener)));
}

void FileSystemManagerImpl::ContinueReadDirectory(
    const storage::FileSystemURL& url,
    mojo::Remote<blink::mojom::FileSystemOperationListener> listener,
    bool security_check_success) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!security_check_success) {
    listener->ErrorOccurred(base::File::FILE_ERROR_SECURITY);
    return;
  }

  OperationListenerID listener_id = AddOpListener(std::move(listener));
  storage::FileSystemOperationRunner* fs_op_runner = operation_runner();
  if (!fs_op_runner) {
    /* A null FileSystemOperationRunner at this point means the corresponding
     * renderer was terminated, so return early to ignore the requested
     * FileSystemOperation. */
    return;
  }

  fs_op_runner->ReadDirectory(
      url, base::BindRepeating(&FileSystemManagerImpl::DidReadDirectory,
                               GetWeakPtr(), listener_id));
}

void FileSystemManagerImpl::ReadDirectorySync(
    const GURL& path,
    ReadDirectorySyncCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  FileSystemURL url(context_->CrackURL(path, receivers_.current_context()));
  std::optional<base::File::Error> opt_error = ValidateFileSystemURL(url);
  if (opt_error) {
    std::move(callback).Run(std::vector<filesystem::mojom::DirectoryEntryPtr>(),
                            opt_error.value());
    return;
  }
  GetUIThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &ChildProcessSecurityPolicyImpl::CanReadFileSystemFile,
          base::Unretained(ChildProcessSecurityPolicyImpl::GetInstance()),
          process_id_, url),
      base::BindOnce(&FileSystemManagerImpl::ContinueReadDirectorySync,
                     weak_factory_.GetWeakPtr(), url, std::move(callback)));
}

void FileSystemManagerImpl::ContinueReadDirectorySync(
    const storage::FileSystemURL& url,
    ReadDirectorySyncCallback callback,
    bool security_check_success) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!security_check_success) {
    std::move(callback).Run(std::vector<filesystem::mojom::DirectoryEntryPtr>(),
                            base::File::FILE_ERROR_SECURITY);
    return;
  }

  storage::FileSystemOperationRunner* fs_op_runner = operation_runner();
  if (!fs_op_runner) {
    /* A null FileSystemOperationRunner at this point means the corresponding
     * renderer was terminated, so return early to ignore the requested
     * FileSystemOperation. */
    return;
  }

  fs_op_runner->ReadDirectory(
      url, base::BindRepeating(
               &FileSystemManagerImpl::DidReadDirectorySync, GetWeakPtr(),
               base::Owned(
                   new ReadDirectorySyncCallbackEntry(std::move(callback)))));
}

void FileSystemManagerImpl::Write(
    const GURL& file_path,
    mojo::PendingRemote<blink::mojom::Blob> blob,
    int64_t position,
    mojo::PendingReceiver<blink::mojom::FileSystemCancellableOperation>
        op_receiver,
    mojo::PendingRemote<blink::mojom::FileSystemOperationListener>
        pending_listener) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  FileSystemURL url(
      context_->CrackURL(file_path, receivers_.current_context()));
  std::optional<base::File::Error> opt_error = ValidateFileSystemURL(url);
  mojo::Remote<blink::mojom::FileSystemOperationListener> listener(
      std::move(pending_listener));
  if (opt_error) {
    listener->ErrorOccurred(opt_error.value());
    return;
  }
  GetUIThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &ChildProcessSecurityPolicyImpl::CanWriteFileSystemFile,
          base::Unretained(ChildProcessSecurityPolicyImpl::GetInstance()),
          process_id_, url),
      base::BindOnce(
          &FileSystemManagerImpl::ResolveBlobForWrite,
          weak_factory_.GetWeakPtr(), std::move(blob),
          base::BindOnce(&FileSystemManagerImpl::ContinueWrite,
                         weak_factory_.GetWeakPtr(), url, position,
                         std::move(op_receiver), std::move(listener))));
}

void FileSystemManagerImpl::ResolveBlobForWrite(
    mojo::PendingRemote<blink::mojom::Blob> blob,
    base::OnceCallback<void(std::unique_ptr<storage::BlobDataHandle>)> callback,
    bool security_check_success) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!security_check_success) {
    std::move(callback).Run(nullptr);
    return;
  }

  blob_storage_context_->context()->GetBlobDataFromBlobRemote(
      std::move(blob), std::move(callback));
}

void FileSystemManagerImpl::ContinueWrite(
    const storage::FileSystemURL& url,
    int64_t position,
    mojo::PendingReceiver<blink::mojom::FileSystemCancellableOperation>
        op_receiver,
    mojo::Remote<blink::mojom::FileSystemOperationListener> listener,
    std::unique_ptr<storage::BlobDataHandle> blob) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!blob) {
    listener->ErrorOccurred(base::File::FILE_ERROR_SECURITY);
    return;
  }

  OperationListenerID listener_id = AddOpListener(std::move(listener));

  storage::FileSystemOperationRunner* fs_op_runner = operation_runner();
  if (!fs_op_runner) {
    /* A null FileSystemOperationRunner at this point means the corresponding
     * renderer was terminated, so return early to ignore the requested
     * FileSystemOperation. */
    return;
  }

  OperationID op_id =
      fs_op_runner->Write(url, std::move(blob), position,
                          base::BindRepeating(&FileSystemManagerImpl::DidWrite,
                                              GetWeakPtr(), listener_id));
  cancellable_operations_.Add(
      std::make_unique<FileSystemCancellableOperationImpl>(op_id, this),
      std::move(op_receiver));
}

void FileSystemManagerImpl::WriteSync(
    const GURL& file_path,
    mojo::PendingRemote<blink::mojom::Blob> blob,
    int64_t position,
    WriteSyncCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  FileSystemURL url(
      context_->CrackURL(file_path, receivers_.current_context()));
  std::optional<base::File::Error> opt_error = ValidateFileSystemURL(url);
  if (opt_error) {
    std::move(callback).Run(0, opt_error.value());
    return;
  }
  GetUIThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &ChildProcessSecurityPolicyImpl::CanWriteFileSystemFile,
          base::Unretained(ChildProcessSecurityPolicyImpl::GetInstance()),
          process_id_, url),
      base::BindOnce(&FileSystemManagerImpl::ResolveBlobForWrite,
                     weak_factory_.GetWeakPtr(), std::move(blob),
                     base::BindOnce(&FileSystemManagerImpl::ContinueWriteSync,
                                    weak_factory_.GetWeakPtr(), url, position,
                                    std::move(callback))));
}

void FileSystemManagerImpl::ContinueWriteSync(
    const storage::FileSystemURL& url,
    int64_t position,
    WriteSyncCallback callback,
    std::unique_ptr<storage::BlobDataHandle> blob) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!blob) {
    std::move(callback).Run(0, base::File::FILE_ERROR_SECURITY);
    return;
  }

  storage::FileSystemOperationRunner* fs_op_runner = operation_runner();
  if (!fs_op_runner) {
    /* A null FileSystemOperationRunner at this point means the corresponding
     * renderer was terminated, so return early to ignore the requested
     * FileSystemOperation. */
    return;
  }

  fs_op_runner->Write(
      url, std::move(blob), position,
      base::BindRepeating(
          &FileSystemManagerImpl::DidWriteSync, GetWeakPtr(),
          base::Owned(new WriteSyncCallbackEntry(std::move(callback)))));
}

void FileSystemManagerImpl::Truncate(
    const GURL& file_path,
    int64_t length,
    mojo::PendingReceiver<blink::mojom::FileSystemCancellableOperation>
        op_receiver,
    TruncateCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  FileSystemURL url(
      context_->CrackURL(file_path, receivers_.current_context()));
  std::optional<base::File::Error> opt_error = ValidateFileSystemURL(url);
  if (opt_error) {
    std::move(callback).Run(opt_error.value());
    return;
  }
  GetUIThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &ChildProcessSecurityPolicyImpl::CanWriteFileSystemFile,
          base::Unretained(ChildProcessSecurityPolicyImpl::GetInstance()),
          process_id_, url),
      base::BindOnce(&FileSystemManagerImpl::ContinueTruncate,
                     weak_factory_.GetWeakPtr(), url, length,
                     std::move(op_receiver), std::move(callback)));
}

void FileSystemManagerImpl::ContinueTruncate(
    const storage::FileSystemURL& url,
    int64_t length,
    mojo::PendingReceiver<blink::mojom::FileSystemCancellableOperation>
        op_receiver,
    TruncateCallback callback,
    bool security_check_success) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!security_check_success) {
    std::move(callback).Run(base::File::FILE_ERROR_SECURITY);
    return;
  }

  storage::FileSystemOperationRunner* fs_op_runner = operation_runner();
  if (!fs_op_runner) {
    /* A null FileSystemOperationRunner at this point means the corresponding
     * renderer was terminated, so return early to ignore the requested
     * FileSystemOperation. */
    return;
  }

  OperationID op_id =
      fs_op_runner->Truncate(url, length,
                             base::BindOnce(&FileSystemManagerImpl::DidFinish,
                                            GetWeakPtr(), std::move(callback)));
  cancellable_operations_.Add(
      std::make_unique<FileSystemCancellableOperationImpl>(op_id, this),
      std::move(op_receiver));
}

void FileSystemManagerImpl::TruncateSync(const GURL& file_path,
                                         int64_t length,
                                         TruncateSyncCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  FileSystemURL url(
      context_->CrackURL(file_path, receivers_.current_context()));
  std::optional<base::File::Error> opt_error = ValidateFileSystemURL(url);
  if (opt_error) {
    std::move(callback).Run(opt_error.value());
    return;
  }

  GetUIThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &ChildProcessSecurityPolicyImpl::CanWriteFileSystemFile,
          base::Unretained(ChildProcessSecurityPolicyImpl::GetInstance()),
          process_id_, url),
      base::BindOnce(&FileSystemManagerImpl::ContinueTruncateSync,
                     weak_factory_.GetWeakPtr(), url, length,
                     std::move(callback)));
}

void FileSystemManagerImpl::ContinueTruncateSync(
    const storage::FileSystemURL& url,
    int64_t length,
    TruncateSyncCallback callback,
    bool security_check_success) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!security_check_success) {
    std::move(callback).Run(base::File::FILE_ERROR_SECURITY);
    return;
  }

  storage::FileSystemOperationRunner* fs_op_runner = operation_runner();
  if (!fs_op_runner) {
    /* A null FileSystemOperationRunner at this point means the corresponding
     * renderer was terminated, so return early to ignore the requested
     * FileSystemOperation. */
    return;
  }

  fs_op_runner->Truncate(url, length,
                         base::BindOnce(&FileSystemManagerImpl::DidFinish,
                                        GetWeakPtr(), std::move(callback)));
}

void FileSystemManagerImpl::CreateSnapshotFile(
    const GURL& file_path,
    CreateSnapshotFileCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  FileSystemURL url(
      context_->CrackURL(file_path, receivers_.current_context()));

  // Make sure if this file can be read by the renderer as this is
  // called when the renderer is about to create a new File object
  // (for reading the file).
  std::optional<base::File::Error> opt_error = ValidateFileSystemURL(url);
  if (opt_error) {
    std::move(callback).Run(base::File::Info(), base::FilePath(),
                            opt_error.value(), mojo::NullRemote());
    return;
  }
  GetUIThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &ChildProcessSecurityPolicyImpl::CanReadFileSystemFile,
          base::Unretained(ChildProcessSecurityPolicyImpl::GetInstance()),
          process_id_, url),
      base::BindOnce(&FileSystemManagerImpl::ContinueCreateSnapshotFile,
                     weak_factory_.GetWeakPtr(), url, std::move(callback)));
}

void FileSystemManagerImpl::ContinueCreateSnapshotFile(
    const storage::FileSystemURL& url,
    CreateSnapshotFileCallback callback,
    bool security_check_success) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!security_check_success) {
    std::move(callback).Run(base::File::Info(), base::FilePath(),
                            base::File::FILE_ERROR_SECURITY,
                            mojo::NullRemote());
    return;
  }

  storage::FileSystemOperationRunner* fs_op_runner = operation_runner();
  if (!fs_op_runner) {
    /* A null FileSystemOperationRunner at this point means the corresponding
     * renderer was terminated, so return early to ignore the requested
     * FileSystemOperation. */
    return;
  }

  FileSystemBackend* backend = context_->GetFileSystemBackend(url.type());
  if (backend->SupportsStreaming(url)) {
    fs_op_runner->GetMetadata(
        url,
        {storage::FileSystemOperation::GetMetadataField::kIsDirectory,
         storage::FileSystemOperation::GetMetadataField::kSize,
         storage::FileSystemOperation::GetMetadataField::kLastModified},
        base::BindOnce(&FileSystemManagerImpl::DidGetMetadataForStreaming,
                       GetWeakPtr(), std::move(callback)));
  } else {
    fs_op_runner->CreateSnapshotFile(
        url, base::BindOnce(&FileSystemManagerImpl::DidCreateSnapshot,
                            GetWeakPtr(), std::move(callback), url));
  }
}

void FileSystemManagerImpl::GetPlatformPath(const GURL& path,
                                            GetPlatformPathCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  base::FilePath platform_path;
  context_->default_file_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&FileSystemManagerImpl::GetPlatformPathOnFileThread, path,
                     process_id_, context_, GetWeakPtr(),
                     receivers_.current_context(), std::move(callback)));
}

void FileSystemManagerImpl::RegisterBlob(
    const std::string& content_type,
    const GURL& url,
    uint64_t length,
    std::optional<base::Time> expected_modification_time,
    RegisterBlobCallback callback) {
  storage::FileSystemURL crack_url =
      context_->CrackURL(url, receivers_.current_context());

  GetUIThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &ChildProcessSecurityPolicyImpl::CanReadFileSystemFile,
          base::Unretained(ChildProcessSecurityPolicyImpl::GetInstance()),
          process_id_, crack_url),
      base::BindOnce(&FileSystemManagerImpl::ContinueRegisterBlob,
                     weak_factory_.GetWeakPtr(), content_type, url, length,
                     expected_modification_time, std::move(callback),
                     crack_url));
}

void FileSystemManagerImpl::ContinueRegisterBlob(
    const std::string& content_type,
    const GURL& url,
    uint64_t length,
    std::optional<base::Time> expected_modification_time,
    RegisterBlobCallback callback,
    storage::FileSystemURL crack_url,
    bool security_check_success) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  std::string uuid = base::Uuid::GenerateRandomV4().AsLowercaseString();
  mojo::PendingRemote<blink::mojom::Blob> blob_remote;
  mojo::PendingReceiver<blink::mojom::Blob> blob_receiver =
      blob_remote.InitWithNewPipeAndPassReceiver();

  if (crack_url.is_valid() &&
      context_->GetFileSystemBackend(crack_url.type()) &&
      security_check_success) {
    blob_storage_context_->CreateFileSystemBlob(
        context_, std::move(blob_receiver), crack_url, uuid, content_type,
        length, expected_modification_time.value_or(base::Time()));
  } else {
    std::unique_ptr<storage::BlobDataHandle> handle =
        blob_storage_context_->context()->AddBrokenBlob(
            uuid, content_type, "",
            storage::BlobStatus::ERR_REFERENCED_FILE_UNAVAILABLE);
    storage::BlobImpl::Create(std::move(handle), std::move(blob_receiver));
  }

  std::move(callback).Run(blink::mojom::SerializedBlob::New(
      uuid, content_type, length, std::move(blob_remote)));
}

void FileSystemManagerImpl::Cancel(
    OperationID op_id,
    FileSystemCancellableOperationImpl::CancelCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  operation_runner()->Cancel(
      op_id, base::BindOnce(&FileSystemManagerImpl::DidFinish, GetWeakPtr(),
                            std::move(callback)));
}

void FileSystemManagerImpl::DidReceiveSnapshotFile(int snapshot_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  in_transit_snapshot_files_.Remove(snapshot_id);
}

void FileSystemManagerImpl::OnConnectionError() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (receivers_.empty()) {
    in_transit_snapshot_files_.Clear();
    operation_runner_.reset();
    cancellable_operations_.Clear();
  }
}

void FileSystemManagerImpl::DidFinish(
    base::OnceCallback<void(base::File::Error)> callback,
    base::File::Error error_code) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  std::move(callback).Run(error_code);
}

void FileSystemManagerImpl::DidGetMetadata(ReadMetadataCallback callback,
                                           base::File::Error result,
                                           const base::File::Info& info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  std::move(callback).Run(info, result);
}

void FileSystemManagerImpl::DidGetMetadataForStreaming(
    CreateSnapshotFileCallback callback,
    base::File::Error result,
    const base::File::Info& info) {
  // For now, streaming Blobs are implemented as a successful snapshot file
  // creation with an empty path.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  std::move(callback).Run(info, base::FilePath(), result, mojo::NullRemote());
}

void FileSystemManagerImpl::DidReadDirectory(
    OperationListenerID listener_id,
    base::File::Error result,
    std::vector<filesystem::mojom::DirectoryEntry> entries,
    bool has_more) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  blink::mojom::FileSystemOperationListener* listener =
      GetOpListener(listener_id);
  if (!listener)
    return;
  if (result != base::File::FILE_OK) {
    DCHECK(!has_more);
    listener->ErrorOccurred(result);
    RemoveOpListener(listener_id);
    return;
  }
  std::vector<filesystem::mojom::DirectoryEntryPtr> entry_struct_ptrs;
  for (const auto& entry : entries) {
    entry_struct_ptrs.emplace_back(
        filesystem::mojom::DirectoryEntry::New(entry));
  }
  listener->ResultsRetrieved(std::move(entry_struct_ptrs), has_more);
  if (!has_more)
    RemoveOpListener(listener_id);
}

void FileSystemManagerImpl::DidReadDirectorySync(
    ReadDirectorySyncCallbackEntry* callback_entry,
    base::File::Error result,
    std::vector<filesystem::mojom::DirectoryEntry> entries,
    bool has_more) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  for (const auto& entry : entries) {
    callback_entry->entries.emplace_back(
        filesystem::mojom::DirectoryEntry::New(std::move(entry)));
  }
  if (result != base::File::FILE_OK || !has_more) {
    std::move(callback_entry->callback)
        .Run(std::move(callback_entry->entries), result);
  }
}

void FileSystemManagerImpl::DidWrite(OperationListenerID listener_id,
                                     base::File::Error result,
                                     int64_t bytes,
                                     bool complete) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  blink::mojom::FileSystemOperationListener* listener =
      GetOpListener(listener_id);
  if (!listener)
    return;
  if (result == base::File::FILE_OK) {
    listener->DidWrite(bytes, complete);
    if (complete)
      RemoveOpListener(listener_id);
  } else {
    listener->ErrorOccurred(result);
    RemoveOpListener(listener_id);
  }
}

void FileSystemManagerImpl::DidWriteSync(WriteSyncCallbackEntry* entry,
                                         base::File::Error result,
                                         int64_t bytes,
                                         bool complete) {
  entry->bytes += bytes;
  if (complete || result != base::File::FILE_OK)
    std::move(entry->callback).Run(entry->bytes, result);
}

void FileSystemManagerImpl::DidOpenFileSystem(
    OpenCallback callback,
    const FileSystemURL& root,
    const std::string& filesystem_name,
    base::File::Error result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(root.is_valid() || result != base::File::FILE_OK);
  std::move(callback).Run(filesystem_name, root.ToGURL(), result);
  // For OpenFileSystem we do not create a new operation, so no unregister here.
}

void FileSystemManagerImpl::DidResolveURL(
    ResolveURLCallback callback,
    base::File::Error result,
    const storage::FileSystemInfo& info,
    const base::FilePath& file_path,
    storage::FileSystemContext::ResolvedEntryType type) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (result == base::File::FILE_OK &&
      type == storage::FileSystemContext::RESOLVED_ENTRY_NOT_FOUND)
    result = base::File::FILE_ERROR_NOT_FOUND;

  base::FilePath normalized_path(
      storage::VirtualPath::GetNormalizedFilePath(file_path));
  std::move(callback).Run(
      ToMojoFileSystemInfo(info), std::move(normalized_path),
      type == storage::FileSystemContext::RESOLVED_ENTRY_DIRECTORY, result);
  // For ResolveURL we do not create a new operation, so no unregister here.
}

void FileSystemManagerImpl::DidCreateSnapshot(
    CreateSnapshotFileCallback callback,
    const storage::FileSystemURL& url,
    base::File::Error result,
    const base::File::Info& info,
    const base::FilePath& platform_path,
    scoped_refptr<storage::ShareableFileReference> /* unused */) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (result != base::File::FILE_OK) {
    std::move(callback).Run(base::File::Info(), base::FilePath(), result,
                            mojo::NullRemote());
    return;
  }

  // Post a task to use ChildProcessSecurityPolicy to check and grant file read
  // permission on the UI thread, since access to these functions on the IO
  // thread should be avoided.
  GetUIThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](ChildProcessSecurityPolicyImpl* security_policy, int process_id,
             const base::FilePath& platform_path) {
            bool can_read_file =
                security_policy->CanReadFile(process_id, platform_path);
            if (!can_read_file) {
              // Give per-file read permission to the snapshot file if it hasn't
              // it yet. In order for the renderer to be able to read the file
              // via File object, it must be granted per-file read permission
              // for the file's platform path. By now, it has already been
              // verified that the renderer has sufficient permissions to read
              // the file, so giving per-file permission here must be safe.
              security_policy->GrantReadFile(process_id, platform_path);
            }
            return can_read_file;
          },
          // ChildProcessSecurityPolicyImpl::GetInstance() is a singleton so
          // refcounting is unnecessary.
          base::Unretained(ChildProcessSecurityPolicyImpl::GetInstance()),
          process_id_, platform_path),
      base::BindOnce(&FileSystemManagerImpl::ContinueDidCreateSnapshot,
                     weak_factory_.GetWeakPtr(), std::move(callback), url,
                     result, info, platform_path));
}

void FileSystemManagerImpl::ContinueDidCreateSnapshot(
    CreateSnapshotFileCallback callback,
    const storage::FileSystemURL& url,
    base::File::Error result,
    const base::File::Info& info,
    const base::FilePath& platform_path,
    bool security_check_success) {
  scoped_refptr<storage::ShareableFileReference> file_ref =
      storage::ShareableFileReference::Get(platform_path);

  if (!security_check_success) {
    // Revoke all permissions for the file when the last ref of the file
    // is dropped.
    if (!file_ref.get()) {
      // Create a reference for temporary permission handling.
      file_ref = storage::ShareableFileReference::GetOrCreate(
          platform_path,
          storage::ShareableFileReference::DONT_DELETE_ON_FINAL_RELEASE,
          context_->default_file_task_runner());
    }
    file_ref->AddFinalReleaseCallback(
        base::BindOnce(&RevokeFilePermission, process_id_));
  }

  if (file_ref.get()) {
    // This ref is held until DidReceiveSnapshotFile is called.
    int request_id = in_transit_snapshot_files_.Add(file_ref);
    mojo::PendingRemote<blink::mojom::ReceivedSnapshotListener> listener;
    snapshot_listeners_.Add(
        std::make_unique<ReceivedSnapshotListenerImpl>(request_id, this),
        listener.InitWithNewPipeAndPassReceiver());
    // Return the file info and platform_path.
    std::move(callback).Run(info, platform_path, result, std::move(listener));
    return;
  }

  // Return the file info and platform_path.
  std::move(callback).Run(info, platform_path, result, mojo::NullRemote());
}

void FileSystemManagerImpl::DidGetPlatformPath(
    scoped_refptr<storage::FileSystemContext> /*context*/,
    GetPlatformPathCallback callback,
    base::FilePath platform_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  std::move(callback).Run(platform_path);
}

// static
void FileSystemManagerImpl::GetPlatformPathOnFileThread(
    const GURL& path,
    int process_id,
    scoped_refptr<storage::FileSystemContext> context,
    base::WeakPtr<FileSystemManagerImpl> file_system_manager,
    const blink::StorageKey& storage_key,
    GetPlatformPathCallback callback) {
  DCHECK(context->default_file_task_runner()->RunsTasksInCurrentSequence());

  // Bind `context` to the callback to ensure it stays alive.
  DoGetPlatformPath(
      context, process_id, path, storage_key,
      base::BindOnce(
          [](base::WeakPtr<FileSystemManagerImpl> file_system_manager,
             scoped_refptr<storage::FileSystemContext> context,
             GetPlatformPathCallback callback,
             const base::FilePath& platform_path) {
            GetIOThreadTaskRunner({})->PostTask(
                FROM_HERE,
                base::BindOnce(&FileSystemManagerImpl::DidGetPlatformPath,
                               std::move(file_system_manager),
                               std::move(context), std::move(callback),
                               platform_path));
          },
          std::move(file_system_manager), context, std::move(callback)));
}

std::optional<base::File::Error> FileSystemManagerImpl::ValidateFileSystemURL(
    const storage::FileSystemURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!FileSystemURLIsValid(context_.get(), url))
    return base::File::FILE_ERROR_INVALID_URL;

  return std::nullopt;
}

FileSystemManagerImpl::OperationListenerID FileSystemManagerImpl::AddOpListener(
    mojo::Remote<blink::mojom::FileSystemOperationListener> listener) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  int op_id = next_operation_listener_id_++;
  listener.set_disconnect_handler(
      base::BindOnce(&FileSystemManagerImpl::OnConnectionErrorForOpListeners,
                     base::Unretained(this), op_id));
  op_listeners_[op_id] = std::move(listener);
  return op_id;
}

void FileSystemManagerImpl::RemoveOpListener(OperationListenerID listener_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(op_listeners_.find(listener_id) != op_listeners_.end());
  op_listeners_.erase(listener_id);
}

blink::mojom::FileSystemOperationListener* FileSystemManagerImpl::GetOpListener(
    OperationListenerID listener_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (op_listeners_.find(listener_id) == op_listeners_.end())
    return nullptr;
  return &*op_listeners_[listener_id];
}

void FileSystemManagerImpl::OnConnectionErrorForOpListeners(
    OperationListenerID listener_id) {
  RemoveOpListener(listener_id);
}

}  // namespace content

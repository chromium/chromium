// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system/file_system_manager_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/notreached.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_util.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/services/filesystem/public/mojom/types.mojom.h"
#include "content/browser/bad_message.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/file_system/browser_file_system_helper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"
#include "ipc/ipc_platform_file.h"
#include "net/base/mime_util.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/blob/shareable_file_reference.h"
#include "storage/browser/file_system/file_observers.h"
#include "storage/browser/file_system/file_permission_policy.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/isolated_context.h"
#include "storage/common/file_system/file_system_info.h"
#include "storage/common/file_system/file_system_types.h"
#include "storage/common/file_system/file_system_util.h"
#include "third_party/blink/public/common/features.h"
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
  NOTREACHED();
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
    case storage::FileSystemType::kFileSystemTypeNativeLocal:
    case storage::FileSystemType::kFileSystemTypeRestrictedNativeLocal:
    case storage::FileSystemType::kFileSystemTypeDragged:
    case storage::FileSystemType::kFileSystemTypeNativeMedia:
    case storage::FileSystemType::kFileSystemTypeDeviceMedia:
    case storage::FileSystemType::kFileSystemTypeSyncable:
    case storage::FileSystemType::kFileSystemTypeSyncableForInternalSync:
    case storage::FileSystemType::kFileSystemTypeNativeForPlatformApp:
    case storage::FileSystemType::kFileSystemTypeForTransientFile:
    case storage::FileSystemType::kFileSystemTypePluginPrivate:
    case storage::FileSystemType::kFileSystemTypeCloudDevice:
    case storage::FileSystemType::kFileSystemTypeProvided:
    case storage::FileSystemType::kFileSystemTypeDeviceMediaAsFileStorage:
    case storage::FileSystemType::kFileSystemTypeArcContent:
    case storage::FileSystemType::kFileSystemTypeArcDocumentsProvider:
    case storage::FileSystemType::kFileSystemTypeDriveFs:
    case storage::FileSystemType::kFileSystemTypeSmbFs:
    case storage::FileSystemType::kFileSystemInternalTypeEnumEnd:
      NOTREACHED();
      return blink::mojom::FileSystemType::kTemporary;
  }
  NOTREACHED();
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
  FileSystemManagerImpl* const file_system_manager_impl_;
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
  FileSystemManagerImpl* const file_system_manager_impl_;
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
      security_policy_(ChildProcessSecurityPolicyImpl::GetInstance()),
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
    mojo::PendingReceiver<blink::mojom::FileSystemManager> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!operation_runner_)
    operation_runner_ = context_->CreateFileSystemOperationRunner();
  receivers_.Add(this, std::move(receiver));
}

void FileSystemManagerImpl::Open(const url::Origin& origin,
                                 blink::mojom::FileSystemType file_system_type,
                                 OpenCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!security_policy_->CanAccessDataForOrigin(process_id_, origin)) {
    const std::string& scheme =
        origin.GetTupleOrPrecursorTupleIfOpaque().scheme();
    bool is_http_based_scheme =
        (scheme == url::kHttpsScheme) || (scheme == url::kHttpsScheme);
    UMA_HISTOGRAM_BOOLEAN(
        "SiteIsolation.FileSystemApi.CanAccessDataForOriginFailure."
        "IsHttpBasedScheme",
        is_http_based_scheme);

    if (base::FeatureList::IsEnabled(
            features::kSiteIsolationEnforcementForFileSystemApi)) {
      receivers_.ReportBadMessage("FSMI_OPEN_INVALID_ORIGIN");
      return;
    }
  }

  if (file_system_type == blink::mojom::FileSystemType::kTemporary) {
    RecordAction(base::UserMetricsAction("OpenFileSystemTemporary"));
  } else if (file_system_type == blink::mojom::FileSystemType::kPersistent) {
    RecordAction(base::UserMetricsAction("OpenFileSystemPersistent"));
  }
  context_->OpenFileSystem(
      origin, ToStorageFileSystemType(file_system_type),
      storage::OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
      base::BindOnce(&FileSystemManagerImpl::DidOpenFileSystem, GetWeakPtr(),
                     std::move(callback)));
}

void FileSystemManagerImpl::ResolveURL(const GURL& filesystem_url,
                                       ResolveURLCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  FileSystemURL url(context_->CrackURL(filesystem_url));
  base::Optional<base::File::Error> opt_error = ValidateFileSystemURL(url);
  if (opt_error) {
    std::move(callback).Run(blink::mojom::FileSystemInfo::New(),
                            base::FilePath(), false, opt_error.value());
    return;
  }

  if (!security_policy_->CanReadFileSystemFile(process_id_, url)) {
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
  FileSystemURL src_url(context_->CrackURL(src_path));
  FileSystemURL dest_url(context_->CrackURL(dest_path));
  base::Optional<base::File::Error> opt_error = ValidateFileSystemURL(src_url);
  if (!opt_error)
    opt_error = ValidateFileSystemURL(dest_url);
  if (opt_error) {
    std::move(callback).Run(opt_error.value());
    return;
  }
  if (!security_policy_->CanReadFileSystemFile(process_id_, src_url) ||
      !security_policy_->CanDeleteFileSystemFile(process_id_, src_url) ||
      !security_policy_->CanCreateFileSystemFile(process_id_, dest_url)) {
    std::move(callback).Run(base::File::FILE_ERROR_SECURITY);
    return;
  }

  operation_runner()->Move(
      src_url, dest_url, storage::FileSystemOperation::OPTION_NONE,
      base::BindOnce(&FileSystemManagerImpl::DidFinish, GetWeakPtr(),
                     base::Passed(&callback)));
}

void FileSystemManagerImpl::Copy(const GURL& src_path,
                                 const GURL& dest_path,
                                 CopyCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  FileSystemURL src_url(context_->CrackURL(src_path));
  FileSystemURL dest_url(context_->CrackURL(dest_path));
  base::Optional<base::File::Error> opt_error = ValidateFileSystemURL(src_url);
  if (!opt_error)
    opt_error = ValidateFileSystemURL(dest_url);
  if (opt_error) {
    std::move(callback).Run(opt_error.value());
    return;
  }
  if (!security_policy_->CanReadFileSystemFile(process_id_, src_url) ||
      !security_policy_->CanCopyIntoFileSystemFile(process_id_, dest_url)) {
    std::move(callback).Run(base::File::FILE_ERROR_SECURITY);
    return;
  }

  operation_runner()->Copy(
      src_url, dest_url, storage::FileSystemOperation::OPTION_NONE,
      FileSystemOperation::ERROR_BEHAVIOR_ABORT,
      storage::FileSystemOperationRunner::CopyProgressCallback(),
      base::BindOnce(&FileSystemManagerImpl::DidFinish, GetWeakPtr(),
                     base::Passed(&callback)));
}

void FileSystemManagerImpl::Remove(const GURL& path,
                                   bool recursive,
                                   RemoveCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  FileSystemURL url(context_->CrackURL(path));
  base::Optional<base::File::Error> opt_error = ValidateFileSystemURL(url);
  if (opt_error) {
    std::move(callback).Run(opt_error.value());
    return;
  }
  if (!security_policy_->CanDeleteFileSystemFile(process_id_, url)) {
    std::move(callback).Run(base::File::FILE_ERROR_SECURITY);
    return;
  }

  operation_runner()->Remove(
      url, recursive,
      base::BindOnce(&FileSystemManagerImpl::DidFinish, GetWeakPtr(),
                     base::Passed(&callback)));
}

void FileSystemManagerImpl::ReadMetadata(const GURL& path,
                                         ReadMetadataCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  FileSystemURL url(context_->CrackURL(path));
  base::Optional<base::File::Error> opt_error = ValidateFileSystemURL(url);
  if (opt_error) {
    std::move(callback).Run(base::File::Info(), opt_error.value());
    return;
  }
  if (!security_policy_->CanReadFileSystemFile(process_id_, url)) {
    std::move(callback).Run(base::File::Info(),
                            base::File::FILE_ERROR_SECURITY);
    return;
  }

  operation_runner()->GetMetadata(
      url,
      FileSystemOperation::GET_METADATA_FIELD_IS_DIRECTORY |
          FileSystemOperation::GET_METADATA_FIELD_SIZE |
          FileSystemOperation::GET_METADATA_FIELD_LAST_MODIFIED,
      base::BindOnce(&FileSystemManagerImpl::DidGetMetadata, GetWeakPtr(),
                     base::Passed(&callback)));
}

void FileSystemManagerImpl::Create(const GURL& path,
                                   bool exclusive,
                                   bool is_directory,
                                   bool recursive,
                                   CreateCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  FileSystemURL url(context_->CrackURL(path));
  base::Optional<base::File::Error> opt_error = ValidateFileSystemURL(url);
  if (opt_error) {
    std::move(callback).Run(opt_error.value());
    return;
  }
  if (!security_policy_->CanCreateFileSystemFile(process_id_, url)) {
    std::move(callback).Run(base::File::FILE_ERROR_SECURITY);
    return;
  }

  if (is_directory) {
    operation_runner()->CreateDirectory(
        url, exclusive, recursive,
        base::BindOnce(&FileSystemManagerImpl::DidFinish, GetWeakPtr(),
                       base::Passed(&callback)));
  } else {
    operation_runner()->CreateFile(
        url, exclusive,
        base::BindOnce(&FileSystemManagerImpl::DidFinish, GetWeakPtr(),
                       base::Passed(&callback)));
  }
}

void FileSystemManagerImpl::Exists(const GURL& path,
                                   bool is_directory,
                                   ExistsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  FileSystemURL url(context_->CrackURL(path));
  base::Optional<base::File::Error> opt_error = ValidateFileSystemURL(url);
  if (opt_error) {
    std::move(callback).Run(opt_error.value());
    return;
  }
  if (!security_policy_->CanReadFileSystemFile(process_id_, url)) {
    std::move(callback).Run(base::File::FILE_ERROR_SECURITY);
    return;
  }

  if (is_directory) {
    operation_runner()->DirectoryExists(
        url, base::BindOnce(&FileSystemManagerImpl::DidFinish, GetWeakPtr(),
                            base::Passed(&callback)));
  } else {
    operation_runner()->FileExists(
        url, base::BindOnce(&FileSystemManagerImpl::DidFinish, GetWeakPtr(),
                            base::Passed(&callback)));
  }
}

void FileSystemManagerImpl::ReadDirectory(
    const GURL& path,
    mojo::PendingRemote<blink::mojom::FileSystemOperationListener>
        pending_listener) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  FileSystemURL url(context_->CrackURL(path));
  base::Optional<base::File::Error> opt_error = ValidateFileSystemURL(url);
  mojo::Remote<blink::mojom::FileSystemOperationListener> listener(
      std::move(pending_listener));
  if (opt_error) {
    listener->ErrorOccurred(opt_error.value());
    return;
  }
  if (!security_policy_->CanReadFileSystemFile(process_id_, url)) {
    listener->ErrorOccurred(base::File::FILE_ERROR_SECURITY);
    return;
  }

  OperationListenerID listener_id = AddOpListener(std::move(listener));
  operation_runner()->ReadDirectory(
      url, base::BindRepeating(&FileSystemManagerImpl::DidReadDirectory,
                               GetWeakPtr(), listener_id));
}

void FileSystemManagerImpl::ReadDirectorySync(
    const GURL& path,
    ReadDirectorySyncCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  FileSystemURL url(context_->CrackURL(path));
  base::Optional<base::File::Error> opt_error = ValidateFileSystemURL(url);
  if (opt_error) {
    std::move(callback).Run(std::vector<filesystem::mojom::DirectoryEntryPtr>(),
                            opt_error.value());
    return;
  }
  if (!security_policy_->CanReadFileSystemFile(process_id_, url)) {
    std::move(callback).Run(std::vector<filesystem::mojom::DirectoryEntryPtr>(),
                            base::File::FILE_ERROR_SECURITY);
    return;
  }

  operation_runner()->ReadDirectory(
      url, base::BindRepeating(
               &FileSystemManagerImpl::DidReadDirectorySync, GetWeakPtr(),
               base::Owned(
                   new ReadDirectorySyncCallbackEntry(std::move(callback)))));
}

void FileSystemManagerImpl::Write(
    const GURL& file_path,
    const std::string& blob_uuid,
    int64_t position,
    mojo::PendingReceiver<blink::mojom::FileSystemCancellableOperation>
        op_receiver,
    mojo::PendingRemote<blink::mojom::FileSystemOperationListener>
        pending_listener) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  FileSystemURL url(context_->CrackURL(file_path));
  base::Optional<base::File::Error> opt_error = ValidateFileSystemURL(url);
  mojo::Remote<blink::mojom::FileSystemOperationListener> listener(
      std::move(pending_listener));
  if (opt_error) {
    listener->ErrorOccurred(opt_error.value());
    return;
  }
  if (!security_policy_->CanWriteFileSystemFile(process_id_, url)) {
    listener->ErrorOccurred(base::File::FILE_ERROR_SECURITY);
    return;
  }
  std::unique_ptr<storage::BlobDataHandle> blob =
      blob_storage_context_->context()->GetBlobDataFromUUID(blob_uuid);

  OperationListenerID listener_id = AddOpListener(std::move(listener));

  OperationID op_id = operation_runner()->Write(
      url, std::move(blob), position,
      base::BindRepeating(&FileSystemManagerImpl::DidWrite, GetWeakPtr(),
                          listener_id));
  cancellable_operations_.Add(
      std::make_unique<FileSystemCancellableOperationImpl>(op_id, this),
      std::move(op_receiver));
}

void FileSystemManagerImpl::WriteSync(const GURL& file_path,
                                      const std::string& blob_uuid,
                                      int64_t position,
                                      WriteSyncCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  FileSystemURL url(context_->CrackURL(file_path));
  base::Optional<base::File::Error> opt_error = ValidateFileSystemURL(url);
  if (opt_error) {
    std::move(callback).Run(0, opt_error.value());
    return;
  }
  if (!security_policy_->CanWriteFileSystemFile(process_id_, url)) {
    std::move(callback).Run(0, base::File::FILE_ERROR_SECURITY);
    return;
  }
  std::unique_ptr<storage::BlobDataHandle> blob =
      blob_storage_context_->context()->GetBlobDataFromUUID(blob_uuid);

  operation_runner()->Write(
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
  FileSystemURL url(context_->CrackURL(file_path));
  base::Optional<base::File::Error> opt_error = ValidateFileSystemURL(url);
  if (opt_error) {
    std::move(callback).Run(opt_error.value());
    return;
  }
  if (!security_policy_->CanWriteFileSystemFile(process_id_, url)) {
    std::move(callback).Run(base::File::FILE_ERROR_SECURITY);
    return;
  }

  OperationID op_id = operation_runner()->Truncate(
      url, length,
      base::BindOnce(&FileSystemManagerImpl::DidFinish, GetWeakPtr(),
                     base::Passed(&callback)));
  cancellable_operations_.Add(
      std::make_unique<FileSystemCancellableOperationImpl>(op_id, this),
      std::move(op_receiver));
}

void FileSystemManagerImpl::TruncateSync(const GURL& file_path,
                                         int64_t length,
                                         TruncateSyncCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  FileSystemURL url(context_->CrackURL(file_path));
  base::Optional<base::File::Error> opt_error = ValidateFileSystemURL(url);
  if (opt_error) {
    std::move(callback).Run(opt_error.value());
    return;
  }
  if (!security_policy_->CanWriteFileSystemFile(process_id_, url)) {
    std::move(callback).Run(base::File::FILE_ERROR_SECURITY);
    return;
  }

  operation_runner()->Truncate(
      url, length,
      base::BindOnce(&FileSystemManagerImpl::DidFinish, GetWeakPtr(),
                     base::Passed(&callback)));
}

void FileSystemManagerImpl::CreateSnapshotFile(
    const GURL& file_path,
    CreateSnapshotFileCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  FileSystemURL url(context_->CrackURL(file_path));

  // Make sure if this file can be read by the renderer as this is
  // called when the renderer is about to create a new File object
  // (for reading the file).
  base::Optional<base::File::Error> opt_error = ValidateFileSystemURL(url);
  if (opt_error) {
    std::move(callback).Run(base::File::Info(), base::FilePath(),
                            opt_error.value(), mojo::NullRemote());
    return;
  }
  if (!security_policy_->CanReadFileSystemFile(process_id_, url)) {
    std::move(callback).Run(base::File::Info(), base::FilePath(),
                            base::File::FILE_ERROR_SECURITY,
                            mojo::NullRemote());
    return;
  }

  FileSystemBackend* backend = context_->GetFileSystemBackend(url.type());
  if (backend->SupportsStreaming(url)) {
    operation_runner()->GetMetadata(
        url,
        FileSystemOperation::GET_METADATA_FIELD_IS_DIRECTORY |
            FileSystemOperation::GET_METADATA_FIELD_SIZE |
            FileSystemOperation::GET_METADATA_FIELD_LAST_MODIFIED,
        base::BindOnce(&FileSystemManagerImpl::DidGetMetadataForStreaming,
                       GetWeakPtr(), base::Passed(&callback)));
  } else {
    operation_runner()->CreateSnapshotFile(
        url, base::BindOnce(&FileSystemManagerImpl::DidCreateSnapshot,
                            GetWeakPtr(), base::Passed(&callback), url));
  }
}

void FileSystemManagerImpl::GetPlatformPath(const GURL& path,
                                            GetPlatformPathCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  base::FilePath platform_path;
  context_->default_file_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&FileSystemManagerImpl::GetPlatformPathOnFileThread, path,
                     process_id_, context_, GetWeakPtr(), std::move(callback)));
}

void FileSystemManagerImpl::Cancel(
    OperationID op_id,
    FileSystemCancellableOperationImpl::CancelCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  operation_runner()->Cancel(
      op_id, base::BindOnce(&FileSystemManagerImpl::DidFinish, GetWeakPtr(),
                            base::Passed(&callback)));
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
    const GURL& root,
    const std::string& filesystem_name,
    base::File::Error result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(root.is_valid() || result != base::File::FILE_OK);
  std::move(callback).Run(filesystem_name, root, result);
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

  scoped_refptr<storage::ShareableFileReference> file_ref =
      storage::ShareableFileReference::Get(platform_path);
  if (!security_policy_->CanReadFile(process_id_, platform_path)) {
    // Give per-file read permission to the snapshot file if it hasn't it yet.
    // In order for the renderer to be able to read the file via File object,
    // it must be granted per-file read permission for the file's platform
    // path. By now, it has already been verified that the renderer has
    // sufficient permissions to read the file, so giving per-file permission
    // here must be safe.
    security_policy_->GrantReadFile(process_id_, platform_path);

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

void FileSystemManagerImpl::DidGetPlatformPath(GetPlatformPathCallback callback,
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
    GetPlatformPathCallback callback) {
  DCHECK(context->default_file_task_runner()->RunsTasksInCurrentSequence());

  SyncGetPlatformPath(
      context.get(), process_id, path,
      base::BindOnce(
          [](base::WeakPtr<FileSystemManagerImpl> file_system_manager,
             GetPlatformPathCallback callback,
             const base::FilePath& platform_path) {
            GetIOThreadTaskRunner({})->PostTask(
                FROM_HERE,
                base::BindOnce(&FileSystemManagerImpl::DidGetPlatformPath,
                               std::move(file_system_manager),
                               std::move(callback), platform_path));
          },
          std::move(file_system_manager), std::move(callback)));
}

base::Optional<base::File::Error> FileSystemManagerImpl::ValidateFileSystemURL(
    const storage::FileSystemURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!FileSystemURLIsValid(context_.get(), url))
    return base::File::FILE_ERROR_INVALID_URL;

  // Deny access to files in PluginPrivate FileSystem from JavaScript.
  // TODO(nhiroki): Move this filter somewhere else since this is not for
  // validation.
  if (url.type() == storage::kFileSystemTypePluginPrivate)
    return base::File::FILE_ERROR_SECURITY;

  return base::nullopt;
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

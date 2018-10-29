// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/fileapi/file_system_manager_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/user_metrics.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/services/filesystem/public/interfaces/types.mojom.h"
#include "content/browser/bad_message.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/fileapi/browser_file_system_helper.h"
#include "content/browser/fileapi/file_system_chooser.h"
#include "content/common/fileapi/webblob_messages.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "ipc/ipc_platform_file.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/base/mime_util.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/blob/shareable_file_reference.h"
#include "storage/browser/fileapi/file_observers.h"
#include "storage/browser/fileapi/file_permission_policy.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "storage/browser/fileapi/file_writer_impl.h"
#include "storage/browser/fileapi/isolated_context.h"
#include "storage/common/fileapi/file_system_info.h"
#include "storage/common/fileapi/file_system_type_converters.h"
#include "storage/common/fileapi/file_system_types.h"
#include "storage/common/fileapi/file_system_util.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

using storage::FileSystemFileUtil;
using storage::FileSystemBackend;
using storage::FileSystemOperation;
using storage::FileSystemURL;
using storage::BlobDataBuilder;
using storage::BlobStorageContext;

namespace content {

namespace {

void RevokeFilePermission(int child_id, const base::FilePath& path) {
  ChildProcessSecurityPolicyImpl::GetInstance()->RevokeAllPermissionsForFile(
      child_id, path);
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
  // |file_system_manager_impl| owns |this| through a StrongBindingSet.
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
  // |file_system_manager_impl| owns |this| through a StrongBindingSet.
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
    int frame_id,
    storage::FileSystemContext* file_system_context,
    scoped_refptr<ChromeBlobStorageContext> blob_storage_context)
    : process_id_(process_id),
      frame_id_(frame_id),
      context_(file_system_context),
      security_policy_(ChildProcessSecurityPolicyImpl::GetInstance()),
      blob_storage_context_(blob_storage_context),
      weak_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(context_);
  DCHECK(blob_storage_context);
  bindings_.set_connection_error_handler(base::BindRepeating(
      &FileSystemManagerImpl::OnConnectionError, base::Unretained(this)));
}

FileSystemManagerImpl::~FileSystemManagerImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

base::WeakPtr<FileSystemManagerImpl> FileSystemManagerImpl::GetWeakPtr() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return weak_factory_.GetWeakPtr();
}

void FileSystemManagerImpl::BindRequest(
    blink::mojom::FileSystemManagerRequest request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!operation_runner_)
    operation_runner_ = context_->CreateFileSystemOperationRunner();
  bindings_.AddBinding(this, std::move(request));
}

void FileSystemManagerImpl::Open(const GURL& origin_url,
                                 blink::mojom::FileSystemType file_system_type,
                                 OpenCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (file_system_type == blink::mojom::FileSystemType::kTemporary) {
    RecordAction(base::UserMetricsAction("OpenFileSystemTemporary"));
  } else if (file_system_type == blink::mojom::FileSystemType::kPersistent) {
    RecordAction(base::UserMetricsAction("OpenFileSystemPersistent"));
  }
  context_->OpenFileSystem(
      origin_url, mojo::ConvertTo<storage::FileSystemType>(file_system_type),
      storage::OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
      base::BindOnce(&FileSystemManagerImpl::DidOpenFileSystem, GetWeakPtr(),
                     std::move(callback)));
};

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
      base::BindRepeating(&FileSystemManagerImpl::DidFinish, GetWeakPtr(),
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
      base::BindRepeating(&FileSystemManagerImpl::DidFinish, GetWeakPtr(),
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
      base::BindRepeating(&FileSystemManagerImpl::DidFinish, GetWeakPtr(),
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
      base::BindRepeating(&FileSystemManagerImpl::DidGetMetadata, GetWeakPtr(),
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
        base::BindRepeating(&FileSystemManagerImpl::DidFinish, GetWeakPtr(),
                            base::Passed(&callback)));
  } else {
    operation_runner()->CreateFile(
        url, exclusive,
        base::BindRepeating(&FileSystemManagerImpl::DidFinish, GetWeakPtr(),
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
        url, base::BindRepeating(&FileSystemManagerImpl::DidFinish,
                                 GetWeakPtr(), base::Passed(&callback)));
  } else {
    operation_runner()->FileExists(
        url, base::BindRepeating(&FileSystemManagerImpl::DidFinish,
                                 GetWeakPtr(), base::Passed(&callback)));
  }
}

void FileSystemManagerImpl::ReadDirectory(
    const GURL& path,
    blink::mojom::FileSystemOperationListenerPtr listener) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  FileSystemURL url(context_->CrackURL(path));
  base::Optional<base::File::Error> opt_error = ValidateFileSystemURL(url);
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
    blink::mojom::FileSystemCancellableOperationRequest op_request,
    blink::mojom::FileSystemOperationListenerPtr listener) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  FileSystemURL url(context_->CrackURL(file_path));
  base::Optional<base::File::Error> opt_error = ValidateFileSystemURL(url);
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
  cancellable_operations_.AddBinding(
      std::make_unique<FileSystemCancellableOperationImpl>(op_id, this),
      std::move(op_request));
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
    blink::mojom::FileSystemCancellableOperationRequest op_request,
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
      base::BindRepeating(&FileSystemManagerImpl::DidFinish, GetWeakPtr(),
                          base::Passed(&callback)));
  cancellable_operations_.AddBinding(
      std::make_unique<FileSystemCancellableOperationImpl>(op_id, this),
      std::move(op_request));
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
      base::BindRepeating(&FileSystemManagerImpl::DidFinish, GetWeakPtr(),
                          base::Passed(&callback)));
}

void FileSystemManagerImpl::TouchFile(const GURL& path,
                                      base::Time last_access_time,
                                      base::Time last_modified_time,
                                      TouchFileCallback callback) {
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

  operation_runner()->TouchFile(
      url, last_access_time, last_modified_time,
      base::BindRepeating(&FileSystemManagerImpl::DidFinish, GetWeakPtr(),
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
                            opt_error.value(), nullptr);
    return;
  }
  if (!security_policy_->CanReadFileSystemFile(process_id_, url)) {
    std::move(callback).Run(base::File::Info(), base::FilePath(),
                            base::File::FILE_ERROR_SECURITY, nullptr);
    return;
  }

  FileSystemBackend* backend = context_->GetFileSystemBackend(url.type());
  if (backend->SupportsStreaming(url)) {
    operation_runner()->GetMetadata(
        url,
        FileSystemOperation::GET_METADATA_FIELD_IS_DIRECTORY |
            FileSystemOperation::GET_METADATA_FIELD_SIZE |
            FileSystemOperation::GET_METADATA_FIELD_LAST_MODIFIED,
        base::BindRepeating(&FileSystemManagerImpl::DidGetMetadataForStreaming,
                            GetWeakPtr(), base::Passed(&callback)));
  } else {
    operation_runner()->CreateSnapshotFile(
        url, base::BindRepeating(&FileSystemManagerImpl::DidCreateSnapshot,
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
                     process_id_, base::Unretained(context_), GetWeakPtr(),
                     std::move(callback)));
}

void FileSystemManagerImpl::CreateWriter(const GURL& file_path,
                                         CreateWriterCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  FileSystemURL url(context_->CrackURL(file_path));
  base::Optional<base::File::Error> opt_error = ValidateFileSystemURL(url);
  if (opt_error) {
    std::move(callback).Run(opt_error.value(), nullptr);
    return;
  }
  if (!security_policy_->CanWriteFileSystemFile(process_id_, url)) {
    std::move(callback).Run(base::File::FILE_ERROR_SECURITY, nullptr);
    return;
  }

  blink::mojom::FileWriterPtr writer;
  mojo::MakeStrongBinding(std::make_unique<storage::FileWriterImpl>(
                              url, context_->CreateFileSystemOperationRunner(),
                              blob_storage_context_->context()->AsWeakPtr()),
                          MakeRequest(&writer));
  std::move(callback).Run(base::File::FILE_OK, std::move(writer));
}

void FileSystemManagerImpl::ChooseEntry(
    blink::mojom::ChooseFileSystemEntryType type,
    std::vector<blink::mojom::ChooseFileSystemEntryAcceptsOptionPtr> accepts,
    bool include_accepts_all,
    ChooseEntryCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!base::FeatureList::IsEnabled(blink::features::kWritableFilesAPI)) {
    bindings_.ReportBadMessage("FSMI_WRITABLE_FILES_DISABLED");
    return;
  }

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(
          &FileSystemChooser::CreateAndShow, process_id_, frame_id_, type,
          std::move(accepts), include_accepts_all, std::move(callback),
          base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO})));
}

void FileSystemManagerImpl::Cancel(
    OperationID op_id,
    FileSystemCancellableOperationImpl::CancelCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  operation_runner()->Cancel(
      op_id, base::BindRepeating(&FileSystemManagerImpl::DidFinish,
                                 GetWeakPtr(), base::Passed(&callback)));
}

void FileSystemManagerImpl::DidReceiveSnapshotFile(int snapshot_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  in_transit_snapshot_files_.Remove(snapshot_id);
}

void FileSystemManagerImpl::OnConnectionError() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (bindings_.empty()) {
    in_transit_snapshot_files_.Clear();
    operation_runner_.reset();
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
  std::move(callback).Run(info, base::FilePath(), result, nullptr);
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
      mojo::ConvertTo<blink::mojom::FileSystemInfoPtr>(info),
      std::move(normalized_path),
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
                            nullptr);
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
    blink::mojom::ReceivedSnapshotListenerPtr listener_ptr;
    snapshot_listeners_.AddBinding(
        std::make_unique<ReceivedSnapshotListenerImpl>(request_id, this),
        mojo::MakeRequest<blink::mojom::ReceivedSnapshotListener>(
            &listener_ptr));
    // Return the file info and platform_path.
    std::move(callback).Run(info, platform_path, result,
                            std::move(listener_ptr));
    return;
  }

  // Return the file info and platform_path.
  std::move(callback).Run(info, platform_path, result, nullptr);
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
    storage::FileSystemContext* context,
    base::WeakPtr<FileSystemManagerImpl> file_system_manager,
    GetPlatformPathCallback callback) {
  base::FilePath platform_path;
  SyncGetPlatformPath(context, process_id, path, &platform_path);
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&FileSystemManagerImpl::DidGetPlatformPath,
                     file_system_manager, std::move(callback), platform_path));
}

base::Optional<base::File::Error> FileSystemManagerImpl::ValidateFileSystemURL(
    const storage::FileSystemURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!FileSystemURLIsValid(context_, url))
    return base::File::FILE_ERROR_INVALID_URL;

  // Deny access to files in PluginPrivate FileSystem from JavaScript.
  // TODO(nhiroki): Move this filter somewhere else since this is not for
  // validation.
  if (url.type() == storage::kFileSystemTypePluginPrivate)
    return base::File::FILE_ERROR_SECURITY;

  return base::nullopt;
}

FileSystemManagerImpl::OperationListenerID FileSystemManagerImpl::AddOpListener(
    blink::mojom::FileSystemOperationListenerPtr listener) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  int op_id = next_operation_listener_id_++;
  listener.set_connection_error_handler(
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

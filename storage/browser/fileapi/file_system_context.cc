// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/fileapi/file_system_context.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/url_request/url_request.h"
#include "storage/browser/fileapi/copy_or_move_file_validator.h"
#include "storage/browser/fileapi/external_mount_points.h"
#include "storage/browser/fileapi/file_permission_policy.h"
#include "storage/browser/fileapi/file_stream_reader.h"
#include "storage/browser/fileapi/file_stream_writer.h"
#include "storage/browser/fileapi/file_system_file_util.h"
#include "storage/browser/fileapi/file_system_operation.h"
#include "storage/browser/fileapi/file_system_operation_runner.h"
#include "storage/browser/fileapi/file_system_options.h"
#include "storage/browser/fileapi/file_system_quota_client.h"
#include "storage/browser/fileapi/isolated_context.h"
#include "storage/browser/fileapi/isolated_file_system_backend.h"
#include "storage/browser/fileapi/mount_points.h"
#include "storage/browser/fileapi/quota/quota_reservation.h"
#include "storage/browser/fileapi/sandbox_file_system_backend.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "storage/common/fileapi/file_system_info.h"
#include "storage/common/fileapi/file_system_util.h"
#include "third_party/leveldatabase/leveldb_chrome.h"
#include "url/gurl.h"

using storage::QuotaClient;

namespace storage {

namespace {

QuotaClient* CreateQuotaClient(
    FileSystemContext* context,
    bool is_incognito) {
  return new FileSystemQuotaClient(context, is_incognito);
}

void DidGetMetadataForResolveURL(const base::FilePath& path,
                                 FileSystemContext::ResolveURLCallback callback,
                                 const FileSystemInfo& info,
                                 base::File::Error error,
                                 const base::File::Info& file_info) {
  if (error != base::File::FILE_OK) {
    if (error == base::File::FILE_ERROR_NOT_FOUND) {
      std::move(callback).Run(base::File::FILE_OK, info, path,
                              FileSystemContext::RESOLVED_ENTRY_NOT_FOUND);
    } else {
      std::move(callback).Run(error, FileSystemInfo(), base::FilePath(),
                              FileSystemContext::RESOLVED_ENTRY_NOT_FOUND);
    }
    return;
  }
  std::move(callback).Run(error, info, path,
                          file_info.is_directory
                              ? FileSystemContext::RESOLVED_ENTRY_DIRECTORY
                              : FileSystemContext::RESOLVED_ENTRY_FILE);
}

void RelayResolveURLCallback(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    FileSystemContext::ResolveURLCallback callback,
    base::File::Error result,
    const FileSystemInfo& info,
    const base::FilePath& file_path,
    FileSystemContext::ResolvedEntryType type) {
  task_runner->PostTask(FROM_HERE, base::BindOnce(std::move(callback), result,
                                                  info, file_path, type));
}

}  // namespace

// static
int FileSystemContext::GetPermissionPolicy(FileSystemType type) {
  switch (type) {
    case kFileSystemTypeTemporary:
    case kFileSystemTypePersistent:
    case kFileSystemTypeSyncable:
      return FILE_PERMISSION_SANDBOX;

    case kFileSystemTypeDrive:
    case kFileSystemTypeNativeForPlatformApp:
    case kFileSystemTypeNativeLocal:
    case kFileSystemTypeCloudDevice:
    case kFileSystemTypeProvided:
    case kFileSystemTypeDeviceMediaAsFileStorage:
    case kFileSystemTypeDriveFs:
      return FILE_PERMISSION_USE_FILE_PERMISSION;

    case kFileSystemTypeRestrictedNativeLocal:
    case kFileSystemTypeArcContent:
    case kFileSystemTypeArcDocumentsProvider:
      return FILE_PERMISSION_READ_ONLY |
             FILE_PERMISSION_USE_FILE_PERMISSION;

    case kFileSystemTypeDeviceMedia:
    case kFileSystemTypeNativeMedia:
      return FILE_PERMISSION_USE_FILE_PERMISSION;

    // Following types are only accessed via IsolatedFileSystem, and
    // don't have their own permission policies.
    case kFileSystemTypeDragged:
    case kFileSystemTypeForTransientFile:
    case kFileSystemTypePluginPrivate:
      return FILE_PERMISSION_ALWAYS_DENY;

    // Following types only appear as mount_type, and will not be
    // queried for their permission policies.
    case kFileSystemTypeIsolated:
    case kFileSystemTypeExternal:
      return FILE_PERMISSION_ALWAYS_DENY;

    // Following types should not be used to access files by FileAPI clients.
    case kFileSystemTypeTest:
    case kFileSystemTypeSyncableForInternalSync:
    case kFileSystemInternalTypeEnumEnd:
    case kFileSystemInternalTypeEnumStart:
    case kFileSystemTypeUnknown:
      return FILE_PERMISSION_ALWAYS_DENY;
  }
  NOTREACHED();
  return FILE_PERMISSION_ALWAYS_DENY;
}

FileSystemContext::FileSystemContext(
    base::SingleThreadTaskRunner* io_task_runner,
    base::SequencedTaskRunner* file_task_runner,
    ExternalMountPoints* external_mount_points,
    storage::SpecialStoragePolicy* special_storage_policy,
    storage::QuotaManagerProxy* quota_manager_proxy,
    std::vector<std::unique_ptr<FileSystemBackend>> additional_backends,
    const std::vector<URLRequestAutoMountHandler>& auto_mount_handlers,
    const base::FilePath& partition_path,
    const FileSystemOptions& options)
    : env_override_(options.is_in_memory()
                        ? leveldb_chrome::NewMemEnv("FileSystem")
                        : nullptr),
      io_task_runner_(io_task_runner),
      default_file_task_runner_(file_task_runner),
      quota_manager_proxy_(quota_manager_proxy),
      sandbox_delegate_(
          new SandboxFileSystemBackendDelegate(quota_manager_proxy,
                                               file_task_runner,
                                               partition_path,
                                               special_storage_policy,
                                               options,
                                               env_override_.get())),
      sandbox_backend_(new SandboxFileSystemBackend(sandbox_delegate_.get())),
      plugin_private_backend_(
          new PluginPrivateFileSystemBackend(file_task_runner,
                                             partition_path,
                                             special_storage_policy,
                                             options,
                                             env_override_.get())),
      additional_backends_(std::move(additional_backends)),
      auto_mount_handlers_(auto_mount_handlers),
      external_mount_points_(external_mount_points),
      partition_path_(partition_path),
      is_incognito_(options.is_incognito()),
      operation_runner_(new FileSystemOperationRunner(this)) {
  RegisterBackend(sandbox_backend_.get());
  RegisterBackend(plugin_private_backend_.get());

  for (const auto& backend : additional_backends_)
    RegisterBackend(backend.get());

  // If the embedder's additional backends already provide support for
  // kFileSystemTypeNativeLocal and kFileSystemTypeNativeForPlatformApp then
  // IsolatedFileSystemBackend does not need to handle them. For example, on
  // Chrome OS the additional backend chromeos::FileSystemBackend handles these
  // types.
  isolated_backend_.reset(new IsolatedFileSystemBackend(
      !base::ContainsKey(backend_map_, kFileSystemTypeNativeLocal),
      !base::ContainsKey(backend_map_, kFileSystemTypeNativeForPlatformApp)));
  RegisterBackend(isolated_backend_.get());

  if (quota_manager_proxy) {
    // Quota client assumes all backends have registered.
    quota_manager_proxy->RegisterClient(CreateQuotaClient(
            this, options.is_incognito()));
  }

  sandbox_backend_->Initialize(this);
  isolated_backend_->Initialize(this);
  plugin_private_backend_->Initialize(this);
  for (const auto& backend : additional_backends_)
    backend->Initialize(this);

  // Additional mount points must be added before regular system-wide
  // mount points.
  if (external_mount_points)
    url_crackers_.push_back(external_mount_points);
  url_crackers_.push_back(ExternalMountPoints::GetSystemInstance());
  url_crackers_.push_back(IsolatedContext::GetInstance());
}

bool FileSystemContext::DeleteDataForOriginOnFileTaskRunner(
    const GURL& origin_url) {
  DCHECK(default_file_task_runner()->RunsTasksInCurrentSequence());
  DCHECK(origin_url == origin_url.GetOrigin());

  bool success = true;
  for (auto& type_backend_pair : backend_map_) {
    FileSystemBackend* backend = type_backend_pair.second;
    if (!backend->GetQuotaUtil())
      continue;
    if (backend->GetQuotaUtil()->DeleteOriginDataOnFileTaskRunner(
            this, quota_manager_proxy(), origin_url, type_backend_pair.first) !=
        base::File::FILE_OK) {
      // Continue the loop, but record the failure.
      success = false;
    }
  }

  return success;
}

scoped_refptr<QuotaReservation>
FileSystemContext::CreateQuotaReservationOnFileTaskRunner(
    const GURL& origin_url,
    FileSystemType type) {
  DCHECK(default_file_task_runner()->RunsTasksInCurrentSequence());
  FileSystemBackend* backend = GetFileSystemBackend(type);
  if (!backend || !backend->GetQuotaUtil())
    return scoped_refptr<QuotaReservation>();
  return backend->GetQuotaUtil()->CreateQuotaReservationOnFileTaskRunner(
      origin_url, type);
}

void FileSystemContext::Shutdown() {
  if (!io_task_runner_->RunsTasksInCurrentSequence()) {
    io_task_runner_->PostTask(FROM_HERE,
                              base::BindOnce(&FileSystemContext::Shutdown,
                                             base::WrapRefCounted(this)));
    return;
  }
  operation_runner_->Shutdown();
}

FileSystemQuotaUtil*
FileSystemContext::GetQuotaUtil(FileSystemType type) const {
  FileSystemBackend* backend = GetFileSystemBackend(type);
  if (!backend)
    return nullptr;
  return backend->GetQuotaUtil();
}

AsyncFileUtil* FileSystemContext::GetAsyncFileUtil(
    FileSystemType type) const {
  FileSystemBackend* backend = GetFileSystemBackend(type);
  if (!backend)
    return nullptr;
  return backend->GetAsyncFileUtil(type);
}

CopyOrMoveFileValidatorFactory*
FileSystemContext::GetCopyOrMoveFileValidatorFactory(
    FileSystemType type, base::File::Error* error_code) const {
  DCHECK(error_code);
  *error_code = base::File::FILE_OK;
  FileSystemBackend* backend = GetFileSystemBackend(type);
  if (!backend)
    return nullptr;
  return backend->GetCopyOrMoveFileValidatorFactory(
      type, error_code);
}

FileSystemBackend* FileSystemContext::GetFileSystemBackend(
    FileSystemType type) const {
  auto found = backend_map_.find(type);
  if (found != backend_map_.end())
    return found->second;
  NOTREACHED() << "Unknown filesystem type: " << type;
  return nullptr;
}

WatcherManager* FileSystemContext::GetWatcherManager(
    FileSystemType type) const {
  FileSystemBackend* backend = GetFileSystemBackend(type);
  if (!backend)
    return nullptr;
  return backend->GetWatcherManager(type);
}

bool FileSystemContext::IsSandboxFileSystem(FileSystemType type) const {
  auto found = backend_map_.find(type);
  return found != backend_map_.end() && found->second->GetQuotaUtil();
}

const UpdateObserverList* FileSystemContext::GetUpdateObservers(
    FileSystemType type) const {
  FileSystemBackend* backend = GetFileSystemBackend(type);
  return backend->GetUpdateObservers(type);
}

const ChangeObserverList* FileSystemContext::GetChangeObservers(
    FileSystemType type) const {
  FileSystemBackend* backend = GetFileSystemBackend(type);
  return backend->GetChangeObservers(type);
}

const AccessObserverList* FileSystemContext::GetAccessObservers(
    FileSystemType type) const {
  FileSystemBackend* backend = GetFileSystemBackend(type);
  return backend->GetAccessObservers(type);
}

std::vector<FileSystemType> FileSystemContext::GetFileSystemTypes() const {
  std::vector<FileSystemType> types;
  types.reserve(backend_map_.size());
  for (const auto& type_backend_pair : backend_map_)
    types.push_back(type_backend_pair.first);
  return types;
}

ExternalFileSystemBackend*
FileSystemContext::external_backend() const {
  return static_cast<ExternalFileSystemBackend*>(
      GetFileSystemBackend(kFileSystemTypeExternal));
}

void FileSystemContext::OpenFileSystem(const GURL& origin_url,
                                       FileSystemType type,
                                       OpenFileSystemMode mode,
                                       OpenFileSystemCallback callback) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!callback.is_null());

  if (!FileSystemContext::IsSandboxFileSystem(type)) {
    // Disallow opening a non-sandboxed filesystem.
    std::move(callback).Run(GURL(), std::string(),
                            base::File::FILE_ERROR_SECURITY);
    return;
  }

  FileSystemBackend* backend = GetFileSystemBackend(type);
  if (!backend) {
    std::move(callback).Run(GURL(), std::string(),
                            base::File::FILE_ERROR_SECURITY);
    return;
  }

  backend->ResolveURL(
      CreateCrackedFileSystemURL(origin_url, type, base::FilePath()), mode,
      std::move(callback));
}

void FileSystemContext::ResolveURL(const FileSystemURL& url,
                                   ResolveURLCallback callback) {
  DCHECK(!callback.is_null());

  // If not on IO thread, forward before passing the task to the backend.
  if (!io_task_runner_->RunsTasksInCurrentSequence()) {
    ResolveURLCallback relay_callback = base::BindOnce(
        &RelayResolveURLCallback, base::ThreadTaskRunnerHandle::Get(),
        std::move(callback));
    io_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&FileSystemContext::ResolveURL, this, url,
                                  std::move(relay_callback)));
    return;
  }

  FileSystemBackend* backend = GetFileSystemBackend(url.type());
  if (!backend) {
    std::move(callback).Run(base::File::FILE_ERROR_SECURITY, FileSystemInfo(),
                            base::FilePath(),
                            FileSystemContext::RESOLVED_ENTRY_NOT_FOUND);
    return;
  }

  backend->ResolveURL(
      url, OPEN_FILE_SYSTEM_FAIL_IF_NONEXISTENT,
      base::BindOnce(&FileSystemContext::DidOpenFileSystemForResolveURL, this,
                     url, std::move(callback)));
}

void FileSystemContext::AttemptAutoMountForURLRequest(
    const FileSystemRequestInfo& request_info,
    StatusCallback callback) {
  const FileSystemURL filesystem_url(request_info.url);
  auto copyable_callback = base::AdaptCallbackForRepeating(std::move(callback));
  if (filesystem_url.type() == kFileSystemTypeExternal) {
    for (size_t i = 0; i < auto_mount_handlers_.size(); i++) {
      if (auto_mount_handlers_[i].Run(request_info, filesystem_url,
                                      copyable_callback)) {
        return;
      }
    }
  }
  copyable_callback.Run(base::File::FILE_ERROR_NOT_FOUND);
}

void FileSystemContext::DeleteFileSystem(const GURL& origin_url,
                                         FileSystemType type,
                                         StatusCallback callback) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(origin_url == origin_url.GetOrigin());
  DCHECK(!callback.is_null());

  FileSystemBackend* backend = GetFileSystemBackend(type);
  if (!backend) {
    std::move(callback).Run(base::File::FILE_ERROR_SECURITY);
    return;
  }
  if (!backend->GetQuotaUtil()) {
    std::move(callback).Run(base::File::FILE_ERROR_INVALID_OPERATION);
    return;
  }

  base::PostTaskAndReplyWithResult(
      default_file_task_runner(), FROM_HERE,
      // It is safe to pass Unretained(quota_util) since context owns it.
      base::BindOnce(&FileSystemQuotaUtil::DeleteOriginDataOnFileTaskRunner,
                     base::Unretained(backend->GetQuotaUtil()),
                     base::RetainedRef(this),
                     base::Unretained(quota_manager_proxy()), origin_url, type),
      std::move(callback));
}

std::unique_ptr<storage::FileStreamReader>
FileSystemContext::CreateFileStreamReader(
    const FileSystemURL& url,
    int64_t offset,
    int64_t max_bytes_to_read,
    const base::Time& expected_modification_time) {
  if (!url.is_valid())
    return std::unique_ptr<storage::FileStreamReader>();
  FileSystemBackend* backend = GetFileSystemBackend(url.type());
  if (!backend)
    return std::unique_ptr<storage::FileStreamReader>();
  return backend->CreateFileStreamReader(
      url, offset, max_bytes_to_read, expected_modification_time, this);
}

std::unique_ptr<FileStreamWriter> FileSystemContext::CreateFileStreamWriter(
    const FileSystemURL& url,
    int64_t offset) {
  if (!url.is_valid())
    return std::unique_ptr<FileStreamWriter>();
  FileSystemBackend* backend = GetFileSystemBackend(url.type());
  if (!backend)
    return std::unique_ptr<FileStreamWriter>();
  return backend->CreateFileStreamWriter(url, offset, this);
}

std::unique_ptr<FileSystemOperationRunner>
FileSystemContext::CreateFileSystemOperationRunner() {
  return base::WrapUnique(new FileSystemOperationRunner(this));
}

FileSystemURL FileSystemContext::CrackURL(const GURL& url) const {
  return CrackFileSystemURL(FileSystemURL(url));
}

FileSystemURL FileSystemContext::CreateCrackedFileSystemURL(
    const GURL& origin,
    FileSystemType type,
    const base::FilePath& path) const {
  return CrackFileSystemURL(FileSystemURL(origin, type, path));
}

#if defined(OS_CHROMEOS)
void FileSystemContext::EnableTemporaryFileSystemInIncognito() {
  sandbox_backend_->set_enable_temporary_file_system_in_incognito(true);
}
#endif

bool FileSystemContext::CanServeURLRequest(const FileSystemURL& url) const {
  // We never support accessing files in isolated filesystems via an URL.
  if (url.mount_type() == kFileSystemTypeIsolated)
    return false;
#if defined(OS_CHROMEOS)
  if (url.type() == kFileSystemTypeTemporary &&
      sandbox_backend_->enable_temporary_file_system_in_incognito()) {
    return true;
  }
#endif
  return !is_incognito_ || !FileSystemContext::IsSandboxFileSystem(url.type());
}

void FileSystemContext::OpenPluginPrivateFileSystem(
    const GURL& origin_url,
    FileSystemType type,
    const std::string& filesystem_id,
    const std::string& plugin_id,
    OpenFileSystemMode mode,
    StatusCallback callback) {
  DCHECK(plugin_private_backend_);
  plugin_private_backend_->OpenPrivateFileSystem(
      origin_url, type, filesystem_id, plugin_id, mode, std::move(callback));
}

FileSystemContext::~FileSystemContext() {
  // TODO(crbug.com/823854) This is a leak. Delete env after the backends have
  // been deleted.
  env_override_.release();
}

void FileSystemContext::DeleteOnCorrectSequence() const {
  if (!io_task_runner_->RunsTasksInCurrentSequence() &&
      io_task_runner_->DeleteSoon(FROM_HERE, this)) {
    return;
  }
  delete this;
}

FileSystemOperation* FileSystemContext::CreateFileSystemOperation(
    const FileSystemURL& url, base::File::Error* error_code) {
  if (!url.is_valid()) {
    if (error_code)
      *error_code = base::File::FILE_ERROR_INVALID_URL;
    return nullptr;
  }

  FileSystemBackend* backend = GetFileSystemBackend(url.type());
  if (!backend) {
    if (error_code)
      *error_code = base::File::FILE_ERROR_FAILED;
    return nullptr;
  }

  base::File::Error fs_error = base::File::FILE_OK;
  FileSystemOperation* operation =
      backend->CreateFileSystemOperation(url, this, &fs_error);

  if (error_code)
    *error_code = fs_error;
  return operation;
}

FileSystemURL FileSystemContext::CrackFileSystemURL(
    const FileSystemURL& url) const {
  if (!url.is_valid())
    return FileSystemURL();

  // The returned value in case there is no crackers which can crack the url.
  // This is valid situation for non isolated/external file systems.
  FileSystemURL current = url;

  // File system may be mounted multiple times (e.g., an isolated filesystem on
  // top of an external filesystem). Hence cracking needs to be iterated.
  for (;;) {
    FileSystemURL cracked = current;
    for (size_t i = 0; i < url_crackers_.size(); ++i) {
      if (!url_crackers_[i]->HandlesFileSystemMountType(current.type()))
        continue;
      cracked = url_crackers_[i]->CrackFileSystemURL(current);
      if (cracked.is_valid())
        break;
    }
    if (cracked == current)
      break;
    current = cracked;
  }
  return current;
}

void FileSystemContext::RegisterBackend(FileSystemBackend* backend) {
  const FileSystemType mount_types[] = {
    kFileSystemTypeTemporary,
    kFileSystemTypePersistent,
    kFileSystemTypeIsolated,
    kFileSystemTypeExternal,
  };
  // Register file system backends for public mount types.
  for (size_t j = 0; j < arraysize(mount_types); ++j) {
    if (backend->CanHandleType(mount_types[j])) {
      const bool inserted = backend_map_.insert(
          std::make_pair(mount_types[j], backend)).second;
      DCHECK(inserted);
    }
  }
  // Register file system backends for internal types.
  for (int t = kFileSystemInternalTypeEnumStart + 1;
       t < kFileSystemInternalTypeEnumEnd; ++t) {
    FileSystemType type = static_cast<FileSystemType>(t);
    if (backend->CanHandleType(type)) {
      const bool inserted = backend_map_.insert(
          std::make_pair(type, backend)).second;
      DCHECK(inserted);
    }
  }
}

void FileSystemContext::DidOpenFileSystemForResolveURL(
    const FileSystemURL& url,
    FileSystemContext::ResolveURLCallback callback,
    const GURL& filesystem_root,
    const std::string& filesystem_name,
    base::File::Error error) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());

  if (error != base::File::FILE_OK) {
    std::move(callback).Run(error, FileSystemInfo(), base::FilePath(),
                            FileSystemContext::RESOLVED_ENTRY_NOT_FOUND);
    return;
  }

  storage::FileSystemInfo info(
      filesystem_name, filesystem_root, url.mount_type());

  // Extract the virtual path not containing a filesystem type part from |url|.
  base::FilePath parent = CrackURL(filesystem_root).virtual_path();
  base::FilePath child = url.virtual_path();
  base::FilePath path;

  if (parent.empty()) {
    path = child;
  } else if (parent != child) {
    bool result = parent.AppendRelativePath(child, &path);
    DCHECK(result);
  }

  // TODO(mtomasz): Not all fields should be required for ResolveURL.
  operation_runner()->GetMetadata(
      url,
      FileSystemOperation::GET_METADATA_FIELD_IS_DIRECTORY |
          FileSystemOperation::GET_METADATA_FIELD_SIZE |
          FileSystemOperation::GET_METADATA_FIELD_LAST_MODIFIED,
      base::BindOnce(&DidGetMetadataForResolveURL, path, std::move(callback),
                     info));
}

}  // namespace storage

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/file_system_context.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/types/pass_key.h"
#include "components/file_access/scoped_file_access_delegate.h"
#include "components/services/storage/public/cpp/buckets/bucket_info.h"
#include "components/services/storage/public/cpp/buckets/constants.h"
#include "components/services/storage/public/cpp/quota_client_callback_wrapper.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "storage/browser/file_system/copy_or_move_file_validator.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_permission_policy.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "storage/browser/file_system/file_stream_writer.h"
#include "storage/browser/file_system/file_system_features.h"
#include "storage/browser/file_system/file_system_file_util.h"
#include "storage/browser/file_system/file_system_operation.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/file_system_options.h"
#include "storage/browser/file_system/file_system_quota_client.h"
#include "storage/browser/file_system/file_system_request_info.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/file_system_util.h"
#include "storage/browser/file_system/isolated_context.h"
#include "storage/browser/file_system/isolated_file_system_backend.h"
#include "storage/browser/file_system/mount_points.h"
#include "storage/browser/file_system/quota/quota_reservation.h"
#include "storage/browser/file_system/sandbox_file_system_backend.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "storage/common/file_system/file_system_info.h"
#include "storage/common/file_system/file_system_util.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom-shared.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "third_party/leveldatabase/leveldb_chrome.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace storage {
namespace {

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

    case kFileSystemTypeLocalForPlatformApp:
    case kFileSystemTypeLocal:
    case kFileSystemTypeProvided:
    case kFileSystemTypeDeviceMediaAsFileStorage:
    case kFileSystemTypeDriveFs:
    case kFileSystemTypeArcContent:
    case kFileSystemTypeArcDocumentsProvider:
    case kFileSystemTypeSmbFs:
    case kFileSystemTypeFuseBox:
      return FILE_PERMISSION_USE_FILE_PERMISSION;

    case kFileSystemTypeDeviceMedia:
    case kFileSystemTypeLocalMedia:
      return FILE_PERMISSION_USE_FILE_PERMISSION;

    // Following types are only accessed via IsolatedFileSystem, and
    // don't have their own permission policies.
    case kFileSystemTypeDragged:
    case kFileSystemTypeForTransientFile:
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
}

scoped_refptr<FileSystemContext> FileSystemContext::Create(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<base::SequencedTaskRunner> file_task_runner,
    scoped_refptr<ExternalMountPoints> external_mount_points,
    scoped_refptr<SpecialStoragePolicy> special_storage_policy,
    scoped_refptr<QuotaManagerProxy> quota_manager_proxy,
    std::vector<std::unique_ptr<FileSystemBackend>> additional_backends,
    const std::vector<URLRequestAutoMountHandler>& auto_mount_handlers,
    const base::FilePath& partition_path,
    const FileSystemOptions& options) {
  bool force_override_incognito = base::FeatureList::IsEnabled(
      features::kIncognitoFileSystemContextForTesting);
  FileSystemOptions maybe_overridden_options =
      force_override_incognito
          ? FileSystemOptions(FileSystemOptions::PROFILE_MODE_INCOGNITO,
                              /*force_in_memory=*/true,
                              options.additional_allowed_schemes())
          : options;

  auto context = base::MakeRefCounted<FileSystemContext>(
      std::move(io_task_runner), std::move(file_task_runner),
      std::move(external_mount_points), std::move(special_storage_policy),
      std::move(quota_manager_proxy), std::move(additional_backends),
      auto_mount_handlers, partition_path, maybe_overridden_options,
      base::PassKey<FileSystemContext>());
  context->Initialize();
  return context;
}

FileSystemContext::FileSystemContext(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<base::SequencedTaskRunner> file_task_runner,
    scoped_refptr<ExternalMountPoints> external_mount_points,
    scoped_refptr<SpecialStoragePolicy> special_storage_policy,
    scoped_refptr<QuotaManagerProxy> quota_manager_proxy,
    std::vector<std::unique_ptr<FileSystemBackend>> additional_backends,
    const std::vector<URLRequestAutoMountHandler>& auto_mount_handlers,
    const base::FilePath& partition_path,
    const FileSystemOptions& options,
    base::PassKey<FileSystemContext>)
    : base::RefCountedDeleteOnSequence<FileSystemContext>(io_task_runner),
      env_override_(options.is_in_memory()
                        ? leveldb_chrome::NewMemEnv("FileSystem")
                        : nullptr),
      io_task_runner_(std::move(io_task_runner)),
      default_file_task_runner_(std::move(file_task_runner)),
      quota_manager_proxy_(std::move(quota_manager_proxy)),
      quota_client_(std::make_unique<FileSystemQuotaClient>(this)),
      quota_client_wrapper_(
          std::make_unique<QuotaClientCallbackWrapper>(quota_client_.get())),
      sandbox_delegate_(std::make_unique<SandboxFileSystemBackendDelegate>(
          quota_manager_proxy_.get(),
          default_file_task_runner_.get(),
          partition_path,
          special_storage_policy,
          options,
          env_override_.get())),
      sandbox_backend_(
          std::make_unique<SandboxFileSystemBackend>(sandbox_delegate_.get())),
      additional_backends_(std::move(additional_backends)),
      auto_mount_handlers_(auto_mount_handlers),
      external_mount_points_(std::move(external_mount_points)),
      partition_path_(partition_path),
      is_incognito_(options.is_incognito()),
      operation_runner_(std::make_unique<FileSystemOperationRunner>(
          base::PassKey<FileSystemContext>(),
          this)),
      quota_client_receiver_(
          std::make_unique<mojo::Receiver<mojom::QuotaClient>>(
              quota_client_wrapper_.get())) {
  RegisterBackend(sandbox_backend_.get());

  for (const auto& backend : additional_backends_)
    RegisterBackend(backend.get());

  // If the embedder's additional backends already provide support for
  // kFileSystemTypeLocal and kFileSystemTypeLocalForPlatformApp then
  // IsolatedFileSystemBackend does not need to handle them. For example, on
  // Chrome OS the additional backend ash::FileSystemBackend handles these
  // types.
  isolated_backend_ = std::make_unique<IsolatedFileSystemBackend>(
      !base::Contains(backend_map_, kFileSystemTypeLocal),
      !base::Contains(backend_map_, kFileSystemTypeLocalForPlatformApp));
  RegisterBackend(isolated_backend_.get());
}

void FileSystemContext::Initialize() {
  sandbox_backend_->Initialize(this);
  isolated_backend_->Initialize(this);
  for (const auto& backend : additional_backends_)
    backend->Initialize(this);

  // Additional mount points must be added before regular system-wide
  // mount points.
  if (external_mount_points_)
    url_crackers_.push_back(external_mount_points_.get());
  url_crackers_.push_back(ExternalMountPoints::GetSystemInstance());
  url_crackers_.push_back(IsolatedContext::GetInstance());

  if (!quota_manager_proxy_)
    return;

  // QuotaManagerProxy::RegisterClient() must be called synchronously during
  // DatabaseTracker creation until crbug.com/1182630 is fixed.
  mojo::PendingRemote<mojom::QuotaClient> quota_client_remote;
  mojo::PendingReceiver<mojom::QuotaClient> quota_client_receiver =
      quota_client_remote.InitWithNewPipeAndPassReceiver();
  quota_manager_proxy_->RegisterClient(std::move(quota_client_remote),
                                       QuotaClientType::kFileSystem,
                                       QuotaManagedStorageTypes());

  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<FileSystemContext> self,
             mojo::PendingReceiver<mojom::QuotaClient> receiver) {
            if (!self->quota_client_receiver_) {
              // Shutdown() may be called directly on the IO sequence. If that
              // happens, `quota_client_receiver_` may get reset before this
              // task runs.
              return;
            }
            self->quota_client_receiver_->Bind(std::move(receiver));
          },
          base::RetainedRef(this), std::move(quota_client_receiver)));
}

scoped_refptr<QuotaReservation>
FileSystemContext::CreateQuotaReservationOnFileTaskRunner(
    const blink::StorageKey& storage_key,
    FileSystemType type) {
  DCHECK(default_file_task_runner()->RunsTasksInCurrentSequence());
  FileSystemBackend* backend = GetFileSystemBackend(type);
  if (!backend || !backend->GetQuotaUtil())
    return scoped_refptr<QuotaReservation>();
  return backend->GetQuotaUtil()->CreateQuotaReservationOnFileTaskRunner(
      storage_key, type);
}

void FileSystemContext::Shutdown() {
  if (!io_task_runner_->RunsTasksInCurrentSequence()) {
    io_task_runner_->PostTask(FROM_HERE,
                              base::BindOnce(&FileSystemContext::Shutdown,
                                             base::WrapRefCounted(this)));
    return;
  }

  // The mojo receiver must be destroyed before the instance it calls into is
  // destroyed.
  quota_client_receiver_.reset();
  quota_client_wrapper_.reset();
  quota_client_.reset();

  operation_runner_->Shutdown();
}

FileSystemQuotaUtil* FileSystemContext::GetQuotaUtil(
    FileSystemType type) const {
  FileSystemBackend* backend = GetFileSystemBackend(type);
  if (!backend)
    return nullptr;
  return backend->GetQuotaUtil();
}

AsyncFileUtil* FileSystemContext::GetAsyncFileUtil(FileSystemType type) const {
  FileSystemBackend* backend = GetFileSystemBackend(type);
  if (!backend)
    return nullptr;
  return backend->GetAsyncFileUtil(type);
}

CopyOrMoveFileValidatorFactory*
FileSystemContext::GetCopyOrMoveFileValidatorFactory(
    FileSystemType type,
    base::File::Error* error_code) const {
  DCHECK(error_code);
  *error_code = base::File::FILE_OK;
  FileSystemBackend* backend = GetFileSystemBackend(type);
  if (!backend)
    return nullptr;
  return backend->GetCopyOrMoveFileValidatorFactory(type, error_code);
}

FileSystemBackend* FileSystemContext::GetFileSystemBackend(
    FileSystemType type) const {
  auto found = backend_map_.find(type);
  if (found != backend_map_.end())
    return found->second;
  NOTREACHED() << "Unknown filesystem type: " << type;
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

void FileSystemContext::OpenFileSystem(
    const blink::StorageKey& storage_key,
    const std::optional<storage::BucketLocator>& bucket,
    FileSystemType type,
    OpenFileSystemMode mode,
    OpenFileSystemCallback callback) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!callback.is_null());
  CHECK(!bucket.has_value() || storage_key == bucket->storage_key);

  if (!FileSystemContext::IsSandboxFileSystem(type)) {
    // Disallow opening a non-sandboxed filesystem.
    std::move(callback).Run(FileSystemURL(), std::string(),
                            base::File::FILE_ERROR_SECURITY);
    return;
  }

  // Quota manager isn't provided by all tests.
  if (!quota_manager_proxy()) {
    ResolveURLOnOpenFileSystem(storage_key, bucket, type, mode,
                               std::move(callback));
    return;
  }

  auto got_bucket = base::BindOnce(&FileSystemContext::OnGetOrCreateBucket,
                                   weak_factory_.GetWeakPtr(), storage_key,
                                   type, mode, std::move(callback));
  if (bucket.has_value()) {
    if (!bucket->id) {
      // This branch can be hit if the bucket has been deleted but `BucketHost`
      // is still alive.
      std::move(got_bucket).Run(base::unexpected(QuotaError::kUnknownError));
    } else {
      quota_manager_proxy()->GetBucketById(bucket->id, io_task_runner_.get(),
                                           std::move(got_bucket));
    }
  } else {
    // Ensure default bucket for `storage_key` exists so that Quota API
    // is aware of the usage.
    quota_manager_proxy()->GetOrCreateBucketDeprecated(
        BucketInitParams::ForDefaultBucket(storage_key),
        FileSystemTypeToQuotaStorageType(type), io_task_runner_.get(),
        std::move(got_bucket));
  }
}

void FileSystemContext::OnGetOrCreateBucket(
    const blink::StorageKey& storage_key,
    FileSystemType type,
    OpenFileSystemMode mode,
    OpenFileSystemCallback callback,
    QuotaErrorOr<BucketInfo> result) {
  if (!result.has_value()) {
    std::move(callback).Run(FileSystemURL(), std::string(),
                            base::File::FILE_ERROR_FAILED);
    return;
  }
  ResolveURLOnOpenFileSystem(storage_key, result->ToBucketLocator(), type, mode,
                             std::move(callback));
}

void FileSystemContext::ResolveURLOnOpenFileSystem(
    const blink::StorageKey& storage_key,
    const std::optional<storage::BucketLocator>& bucket,
    FileSystemType type,
    OpenFileSystemMode mode,
    OpenFileSystemCallback callback) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());

  FileSystemBackend* backend = GetFileSystemBackend(type);
  if (!backend) {
    std::move(callback).Run(FileSystemURL(), std::string(),
                            base::File::FILE_ERROR_SECURITY);
    return;
  }

  FileSystemURL url =
      CreateCrackedFileSystemURL(storage_key, type, base::FilePath());
  // Override the default storage bucket if a custom BucketLocator was given.
  if (bucket.has_value()) {
    url.SetBucket(bucket.value());
  }
  // Bind `this` to the callback to ensure this instance stays alive while the
  // URL is resolving.
  backend->ResolveURL(
      url, mode,
      base::BindOnce(&FileSystemContext::DidResolveURLOnOpenFileSystem, this,
                     url, std::move(callback)));
}

void FileSystemContext::DidResolveURLOnOpenFileSystem(
    const FileSystemURL& filesystem_root_url,
    OpenFileSystemCallback callback,
    const GURL& filesystem_root,
    const std::string& filesystem_name,
    base::File::Error error) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());

  std::move(callback).Run(filesystem_root_url, filesystem_name, error);
}

void FileSystemContext::ResolveURL(const FileSystemURL& url,
                                   ResolveURLCallback callback) {
  DCHECK(!callback.is_null());

  // If not on IO thread, forward before passing the task to the backend.
  if (!io_task_runner_->RunsTasksInCurrentSequence()) {
    ResolveURLCallback relay_callback = base::BindOnce(
        &RelayResolveURLCallback,
        base::SingleThreadTaskRunner::GetCurrentDefault(), std::move(callback));
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
  const FileSystemURL filesystem_url(request_info.url,
                                     request_info.storage_key);
  if (filesystem_url.type() == kFileSystemTypeExternal) {
    for (auto& handler : auto_mount_handlers_) {
      auto split_callback = base::SplitOnceCallback(std::move(callback));
      callback = std::move(split_callback.first);
      if (handler.Run(request_info, filesystem_url,
                      std::move(split_callback.second))) {
        // The `callback` will be run if true was returned.
        return;
      }
    }
  }
  // If every handler returned false, then `callback` was not run yet.
  std::move(callback).Run(base::File::FILE_ERROR_NOT_FOUND);
}

void FileSystemContext::DeleteFileSystem(const blink::StorageKey& storage_key,
                                         FileSystemType type,
                                         StatusCallback callback) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!storage_key.origin().opaque());
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

  quota_manager_proxy()->GetOrCreateBucketDeprecated(
      BucketInitParams::ForDefaultBucket(storage_key),
      FileSystemTypeToQuotaStorageType(type), io_task_runner_.get(),
      base::BindOnce(&FileSystemContext::OnGetBucketForDeleteFileSystem,
                     weak_factory_.GetWeakPtr(), type, std::move(callback)));
}

void FileSystemContext::OnGetBucketForDeleteFileSystem(
    FileSystemType type,
    StatusCallback callback,
    QuotaErrorOr<BucketInfo> result) {
  if (!result.has_value()) {
    std::move(callback).Run(base::File::FILE_ERROR_FAILED);
    return;
  }

  FileSystemBackend* backend = GetFileSystemBackend(type);
  default_file_task_runner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      // It is safe to pass Unretained(quota_util) since context owns it.
      base::BindOnce(&FileSystemQuotaUtil::DeleteBucketDataOnFileTaskRunner,
                     base::Unretained(backend->GetQuotaUtil()),
                     base::RetainedRef(this),
                     base::Unretained(quota_manager_proxy().get()),
                     result->ToBucketLocator(), type),
      std::move(callback));
}

std::unique_ptr<FileStreamReader> FileSystemContext::CreateFileStreamReader(
    const FileSystemURL& url,
    int64_t offset,
    int64_t max_bytes_to_read,
    const base::Time& expected_modification_time,
    file_access::ScopedFileAccessDelegate::RequestFilesAccessIOCallback
        file_access) {
  if (!url.is_valid())
    return nullptr;
  FileSystemBackend* backend = GetFileSystemBackend(url.type());
  if (!backend)
    return nullptr;
  return backend->CreateFileStreamReader(url, offset, max_bytes_to_read,
                                         expected_modification_time, this,
                                         std::move(file_access));
}

std::unique_ptr<FileStreamWriter> FileSystemContext::CreateFileStreamWriter(
    const FileSystemURL& url,
    int64_t offset) {
  if (!url.is_valid())
    return nullptr;
  FileSystemBackend* backend = GetFileSystemBackend(url.type());
  if (!backend)
    return nullptr;
  return backend->CreateFileStreamWriter(url, offset, this);
}

std::unique_ptr<FileSystemOperationRunner>
FileSystemContext::CreateFileSystemOperationRunner() {
  return std::make_unique<FileSystemOperationRunner>(
      base::PassKey<FileSystemContext>(), this);
}

base::SequenceBound<FileSystemOperationRunner>
FileSystemContext::CreateSequenceBoundFileSystemOperationRunner() {
  return base::SequenceBound<FileSystemOperationRunner>(
      io_task_runner_, base::PassKey<FileSystemContext>(),
      base::WrapRefCounted(this));
}

FileSystemURL FileSystemContext::CrackURL(
    const GURL& url,
    const blink::StorageKey& storage_key) const {
  return CrackFileSystemURL(FileSystemURL(url, storage_key));
}

FileSystemURL FileSystemContext::CrackURLInFirstPartyContext(
    const GURL& url) const {
  return CrackFileSystemURL(FileSystemURL(
      url, blink::StorageKey::CreateFirstParty(url::Origin::Create(url))));
}

FileSystemURL FileSystemContext::CreateCrackedFileSystemURL(
    const blink::StorageKey& storage_key,
    FileSystemType type,
    const base::FilePath& path) const {
  return CrackFileSystemURL(FileSystemURL(storage_key, type, path));
}

bool FileSystemContext::CanServeURLRequest(const FileSystemURL& url) const {
  // We never support accessing files in isolated filesystems via an URL.
  if (url.mount_type() == kFileSystemTypeIsolated)
    return false;
  if (url.type() == kFileSystemTypeTemporary)
    return true;
  if (url.type() == kFileSystemTypePersistent) {
    return true;
  }
  return !is_incognito_ || !FileSystemContext::IsSandboxFileSystem(url.type());
}

FileSystemContext::~FileSystemContext() {
  // TODO(crbug.com/41377719) This is a leak. Delete env after the backends have
  // been deleted.
  env_override_.release();
}

base::flat_set<blink::mojom::StorageType>
FileSystemContext::QuotaManagedStorageTypes() {
  std::vector<blink::mojom::StorageType> quota_storage_types;
  for (FileSystemType file_system_type : GetFileSystemTypes()) {
    const blink::mojom::StorageType storage_type =
        FileSystemTypeToQuotaStorageType(file_system_type);
    if (storage_type == blink::mojom::StorageType::kTemporary ||
        storage_type == blink::mojom::StorageType::kSyncable) {
      quota_storage_types.push_back(storage_type);
    }
  }
  return base::MakeFlatSet<blink::mojom::StorageType>(quota_storage_types);
}

std::unique_ptr<FileSystemOperation>
FileSystemContext::CreateFileSystemOperation(OperationType type,
                                             const FileSystemURL& url,
                                             base::File::Error* error_code) {
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
  std::unique_ptr<FileSystemOperation> operation =
      backend->CreateFileSystemOperation(type, url, this, &fs_error);

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
    for (MountPoints* url_cracker : url_crackers_) {
      if (!url_cracker->HandlesFileSystemMountType(current.type()))
        continue;
      cracked = url_cracker->CrackFileSystemURL(current);
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
  for (const auto& mount_type : mount_types) {
    if (backend->CanHandleType(mount_type)) {
      const bool inserted =
          backend_map_.insert(std::make_pair(mount_type, backend)).second;
      DCHECK(inserted);
    }
  }
  // Register file system backends for internal types.
  for (int t = kFileSystemInternalTypeEnumStart + 1;
       t < kFileSystemInternalTypeEnumEnd; ++t) {
    FileSystemType type = static_cast<FileSystemType>(t);
    if (backend->CanHandleType(type)) {
      const bool inserted =
          backend_map_.insert(std::make_pair(type, backend)).second;
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

  FileSystemInfo info(filesystem_name, filesystem_root, url.mount_type());

  // Extract the virtual path not containing a filesystem type part from `url`.
  base::FilePath parent =
      CrackURLInFirstPartyContext(filesystem_root).virtual_path();
  base::FilePath child = url.virtual_path();
  base::FilePath path;

  if (parent.empty()) {
    path = child;
  } else if (parent != child) {
    bool result = parent.AppendRelativePath(child, &path);
    DCHECK(result);
  }

  operation_runner()->GetMetadata(
      url, {storage::FileSystemOperation::GetMetadataField::kIsDirectory},
      base::BindOnce(&DidGetMetadataForResolveURL, path, std::move(callback),
                     info));
}

}  // namespace storage

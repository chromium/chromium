// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/plugin_private_file_system_backend.h"

#include <stdint.h>

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/task/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "storage/browser/file_system/async_file_util_adapter.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation.h"
#include "storage/browser/file_system/file_system_operation_context.h"
#include "storage/browser/file_system/isolated_context.h"
#include "storage/browser/file_system/obfuscated_file_util.h"
#include "storage/browser/file_system/obfuscated_file_util_memory_delegate.h"
#include "storage/browser/file_system/quota/quota_reservation.h"
#include "storage/browser/file_system/sandbox_file_stream_reader.h"
#include "storage/browser/file_system/sandbox_file_stream_writer.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "storage/common/file_system/file_system_util.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/origin.h"

namespace storage {

class PluginPrivateFileSystemBackend::FileSystemIDToPluginMap {
 public:
  explicit FileSystemIDToPluginMap(
      scoped_refptr<base::SequencedTaskRunner> task_runner)
      : task_runner_(std::move(task_runner)) {}
  ~FileSystemIDToPluginMap() = default;

  std::string GetPluginIDForURL(const FileSystemURL& url) {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    auto found = map_.find(url.filesystem_id());
    if (url.type() != kFileSystemTypePluginPrivate || found == map_.end()) {
      NOTREACHED() << "Unsupported url is given: " << url.DebugString();
      return std::string();
    }
    return found->second;
  }

  void RegisterFileSystem(const std::string& filesystem_id,
                          const std::string& plugin_id) {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    DCHECK(!filesystem_id.empty());
    DCHECK(!base::Contains(map_, filesystem_id)) << filesystem_id;
    map_[filesystem_id] = plugin_id;
  }

  void RemoveFileSystem(const std::string& filesystem_id) {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    map_.erase(filesystem_id);
  }

 private:
  using Map = std::map<std::string, std::string>;
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  Map map_;
};

namespace {

const base::FilePath::CharType* kFileSystemDirectory =
    SandboxFileSystemBackendDelegate::kFileSystemDirectory;
const base::FilePath::CharType* kPluginPrivateDirectory =
    FILE_PATH_LITERAL("Plugins");

base::File::Error OpenFileSystemOnFileTaskRunner(
    ObfuscatedFileUtil* file_util,
    PluginPrivateFileSystemBackend::FileSystemIDToPluginMap* plugin_map,
    const url::Origin& origin,
    const std::string& filesystem_id,
    const std::string& plugin_id,
    OpenFileSystemMode mode) {
  base::File::Error error = base::File::FILE_ERROR_FAILED;
  const bool create = (mode == OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT);
  // TODO(https://crbug.com/1231162): determine whether EME/CDM/plugin private
  // file system will be partitioned; if so, replace the in-line conversion with
  // the correct third-party StorageKey.
  file_util->GetDirectoryForStorageKeyAndType(blink::StorageKey(origin),
                                              plugin_id, create, &error);
  if (error == base::File::FILE_OK)
    plugin_map->RegisterFileSystem(filesystem_id, plugin_id);
  return error;
}

}  // namespace

PluginPrivateFileSystemBackend::CdmFileInfo::CdmFileInfo(
    const std::string& name,
    const std::string& legacy_file_system_id)
    : name(name), legacy_file_system_id(legacy_file_system_id) {}
PluginPrivateFileSystemBackend::CdmFileInfo::CdmFileInfo(const CdmFileInfo&) =
    default;
PluginPrivateFileSystemBackend::CdmFileInfo::CdmFileInfo(CdmFileInfo&&) =
    default;
PluginPrivateFileSystemBackend::CdmFileInfo::~CdmFileInfo() = default;

PluginPrivateFileSystemBackend::PluginPrivateFileSystemBackend(
    scoped_refptr<base::SequencedTaskRunner> file_task_runner,
    const base::FilePath& profile_path,
    const base::FilePath& bucket_base_path,
    scoped_refptr<SpecialStoragePolicy> special_storage_policy,
    const FileSystemOptions& file_system_options,
    leveldb::Env* env_override)
    : file_task_runner_(std::move(file_task_runner)),
      file_system_options_(file_system_options),
      base_path_(profile_path.Append(kFileSystemDirectory)
                     .Append(kPluginPrivateDirectory)),
      plugin_map_(new FileSystemIDToPluginMap(file_task_runner_)) {
  file_util_ = std::make_unique<AsyncFileUtilAdapter>(
      std::make_unique<ObfuscatedFileUtil>(
          std::move(special_storage_policy), base_path_, bucket_base_path,
          env_override,
          base::BindRepeating(&FileSystemIDToPluginMap::GetPluginIDForURL,
                              base::Owned(plugin_map_.get())),
          std::set<std::string>(), nullptr,
          file_system_options.is_incognito()));
}

PluginPrivateFileSystemBackend::~PluginPrivateFileSystemBackend() {
  if (!file_task_runner_->RunsTasksInCurrentSequence()) {
    AsyncFileUtil* file_util = file_util_.release();
    if (!file_task_runner_->DeleteSoon(FROM_HERE, file_util))
      delete file_util;
  }
}

void PluginPrivateFileSystemBackend::OpenPrivateFileSystem(
    const url::Origin& origin,
    FileSystemType type,
    const std::string& filesystem_id,
    const std::string& plugin_id,
    OpenFileSystemMode mode,
    StatusCallback callback) {
  if (!CanHandleType(type)) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), base::File::FILE_ERROR_SECURITY));
    return;
  }

  PostTaskAndReplyWithResult(
      file_task_runner_.get(), FROM_HERE,
      base::BindOnce(&OpenFileSystemOnFileTaskRunner, obfuscated_file_util(),
                     plugin_map_, origin, filesystem_id, plugin_id, mode),
      std::move(callback));
}

bool PluginPrivateFileSystemBackend::CanHandleType(FileSystemType type) const {
  return type == kFileSystemTypePluginPrivate;
}

void PluginPrivateFileSystemBackend::Initialize(FileSystemContext* context) {}

void PluginPrivateFileSystemBackend::ResolveURL(const FileSystemURL& url,
                                                OpenFileSystemMode mode,
                                                ResolveURLCallback callback) {
  // We never allow opening a new plugin-private filesystem via usual
  // ResolveURL.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), GURL(), std::string(),
                                base::File::FILE_ERROR_SECURITY));
}

AsyncFileUtil* PluginPrivateFileSystemBackend::GetAsyncFileUtil(
    FileSystemType type) {
  return file_util_.get();
}

WatcherManager* PluginPrivateFileSystemBackend::GetWatcherManager(
    FileSystemType type) {
  return nullptr;
}

CopyOrMoveFileValidatorFactory*
PluginPrivateFileSystemBackend::GetCopyOrMoveFileValidatorFactory(
    FileSystemType type,
    base::File::Error* error_code) {
  DCHECK(error_code);
  *error_code = base::File::FILE_OK;
  return nullptr;
}

std::unique_ptr<FileSystemOperation>
PluginPrivateFileSystemBackend::CreateFileSystemOperation(
    const FileSystemURL& url,
    FileSystemContext* context,
    base::File::Error* error_code) const {
  auto operation_context =
      std::make_unique<FileSystemOperationContext>(context);
  return FileSystemOperation::Create(url, context,
                                     std::move(operation_context));
}

bool PluginPrivateFileSystemBackend::SupportsStreaming(
    const FileSystemURL& url) const {
  // Streaming is required for incognito file systems in order to access
  // memory-backed files.
  DCHECK(CanHandleType(url.type()));
  return file_system_options_.is_incognito();
}

bool PluginPrivateFileSystemBackend::HasInplaceCopyImplementation(
    FileSystemType type) const {
  return false;
}

std::unique_ptr<FileStreamReader>
PluginPrivateFileSystemBackend::CreateFileStreamReader(
    const FileSystemURL& url,
    int64_t offset,
    int64_t max_bytes_to_read,
    const base::Time& expected_modification_time,
    FileSystemContext* context) const {
  DCHECK(CanHandleType(url.type()));
  return std::make_unique<SandboxFileStreamReader>(context, url, offset,
                                                   expected_modification_time);
}

std::unique_ptr<FileStreamWriter>
PluginPrivateFileSystemBackend::CreateFileStreamWriter(
    const FileSystemURL& url,
    int64_t offset,
    FileSystemContext* context) const {
  DCHECK(CanHandleType(url.type()));

  // Observers not supported by PluginPrivateFileSystemBackend.
  return std::make_unique<SandboxFileStreamWriter>(context, url, offset,
                                                   UpdateObserverList());
}

FileSystemQuotaUtil* PluginPrivateFileSystemBackend::GetQuotaUtil() {
  return this;
}

base::File::Error
PluginPrivateFileSystemBackend::DeleteStorageKeyDataOnFileTaskRunner(
    FileSystemContext* context,
    QuotaManagerProxy* proxy,
    const blink::StorageKey& storage_key,
    FileSystemType type) {
  if (!CanHandleType(type))
    return base::File::FILE_ERROR_SECURITY;
  bool result = obfuscated_file_util()->DeleteDirectoryForStorageKeyAndType(
      storage_key, std::string());
  if (result)
    return base::File::FILE_OK;
  return base::File::FILE_ERROR_FAILED;
}

void PluginPrivateFileSystemBackend::PerformStorageCleanupOnFileTaskRunner(
    FileSystemContext* context,
    QuotaManagerProxy* proxy,
    FileSystemType type) {
  if (!CanHandleType(type))
    return;
  obfuscated_file_util()->RewriteDatabases();
}

std::vector<blink::StorageKey>
PluginPrivateFileSystemBackend::GetStorageKeysForTypeOnFileTaskRunner(
    FileSystemType type) {
  if (!CanHandleType(type))
    return std::vector<blink::StorageKey>();
  std::unique_ptr<ObfuscatedFileUtil::AbstractStorageKeyEnumerator> enumerator(
      obfuscated_file_util()->CreateStorageKeyEnumerator());
  std::vector<blink::StorageKey> storage_keys;
  absl::optional<blink::StorageKey> storage_key;
  while ((storage_key = enumerator->Next()).has_value())
    storage_keys.push_back(std::move(storage_key).value());
  return storage_keys;
}

int64_t PluginPrivateFileSystemBackend::GetStorageKeyUsageOnFileTaskRunner(
    FileSystemContext* context,
    const blink::StorageKey& storage_key,
    FileSystemType type) {
  DCHECK(file_task_runner_->RunsTasksInCurrentSequence());

  if (!CanHandleType(type))
    return 0;

  int64_t total_size;
  base::Time last_modified_time;
  GetOriginDetailsOnFileTaskRunner(context, storage_key.origin(), &total_size,
                                   &last_modified_time);
  return total_size;
}

void PluginPrivateFileSystemBackend::GetOriginDetailsOnFileTaskRunner(
    FileSystemContext* context,
    const url::Origin& origin,
    int64_t* total_size,
    base::Time* last_modified_time) {
  DCHECK(file_task_runner_->RunsTasksInCurrentSequence());

  *total_size = 0;
  *last_modified_time = base::Time::UnixEpoch();
  std::string fsid =
      IsolatedContext::GetInstance()->RegisterFileSystemForVirtualPath(
          kFileSystemTypePluginPrivate, kPluginPrivateRootName,
          base::FilePath());
  DCHECK(ValidateIsolatedFileSystemId(fsid));

  std::string root = GetIsolatedFileSystemRootURIString(origin.GetURL(), fsid,
                                                        kPluginPrivateRootName);

  std::unique_ptr<FileSystemOperationContext> operation_context(
      std::make_unique<FileSystemOperationContext>(context));

  // Determine the available plugin private filesystem directories for this
  // origin. Currently the plugin private filesystem is only used by Encrypted
  // Media Content Decryption Modules. Each CDM gets a directory based on the
  // mimetype (e.g. plugin application/x-ppapi-widevine-cdm uses directory
  // application_x-ppapi-widevine-cdm). Enumerate through the set of
  // directories so that data from any CDM used by this origin is counted.
  base::File::Error error;
  // TODO(https://crbug.com/1231162): determine whether EME/CDM/plugin private
  // file system will be partitioned; if so, replace the in-line conversion with
  // the correct third-party StorageKey.
  base::FilePath path =
      obfuscated_file_util()->GetDirectoryForStorageKeyAndType(
          blink::StorageKey(origin), "", false, &error);
  if (error != base::File::FILE_OK) {
    DLOG(ERROR) << "Unable to read directory for " << origin;
    return;
  }

  base::FileEnumerator directory_enumerator(path, false,
                                            base::FileEnumerator::DIRECTORIES);
  base::FilePath plugin_path;
  while (!(plugin_path = directory_enumerator.Next()).empty()) {
    std::string plugin_name = plugin_path.BaseName().MaybeAsASCII();
    if (OpenFileSystemOnFileTaskRunner(
            obfuscated_file_util(), plugin_map_, origin, fsid, plugin_name,
            OPEN_FILE_SYSTEM_FAIL_IF_NONEXISTENT) != base::File::FILE_OK) {
      continue;
    }

    // TODO(https://crbug.com/1231162): determine whether EME/CDM/plugin private
    // file system will be partitioned and use the appropriate StorageKey
    std::unique_ptr<FileSystemFileUtil::AbstractFileEnumerator> enumerator(
        obfuscated_file_util()->CreateFileEnumerator(
            operation_context.get(),
            context->CrackURL(
                GURL(root), blink::StorageKey(url::Origin::Create(GURL(root)))),
            true));

    while (!enumerator->Next().empty()) {
      *total_size += enumerator->Size();
      if (enumerator->LastModifiedTime() > *last_modified_time)
        *last_modified_time = enumerator->LastModifiedTime();
    }
  }
}

std::vector<PluginPrivateFileSystemBackend::CdmFileInfo>
PluginPrivateFileSystemBackend::GetMediaLicenseFilesForOriginOnFileTaskRunner(
    FileSystemContext* context,
    const url::Origin& origin) {
  DCHECK(file_task_runner_->RunsTasksInCurrentSequence());

  std::unique_ptr<FileSystemOperationContext> operation_context(
      std::make_unique<FileSystemOperationContext>(context));

  // Determine the available plugin private filesystem directories for this
  // origin. Currently the plugin private filesystem is only used by Encrypted
  // Media Content Decryption Modules. Each CDM gets a directory based on the
  // mimetype (e.g. plugin application/x-ppapi-widevine-cdm uses directory
  // application_x-ppapi-widevine-cdm). Enumerate through the set of
  // directories so that data from any CDM used by this origin is counted.
  base::File::Error error;
  base::FilePath path =
      obfuscated_file_util()->GetDirectoryForStorageKeyAndType(
          blink::StorageKey(origin), "", false, &error);
  if (error != base::File::FILE_OK)
    return {};

  std::vector<CdmFileInfo> cdm_files;
  base::FileEnumerator directory_enumerator(path, false,
                                            base::FileEnumerator::DIRECTORIES);
  base::FilePath plugin_path;
  while (!(plugin_path = directory_enumerator.Next()).empty()) {
    std::string plugin_name = plugin_path.BaseName().MaybeAsASCII();

    std::string fsid =
        IsolatedContext::GetInstance()->RegisterFileSystemForVirtualPath(
            kFileSystemTypePluginPrivate, kPluginPrivateRootName,
            base::FilePath());
    DCHECK(ValidateIsolatedFileSystemId(fsid));
    std::string root = GetIsolatedFileSystemRootURIString(
        origin.GetURL(), fsid, kPluginPrivateRootName);

    if (OpenFileSystemOnFileTaskRunner(
            obfuscated_file_util(), plugin_map_, origin, fsid, plugin_name,
            OPEN_FILE_SYSTEM_FAIL_IF_NONEXISTENT) != base::File::FILE_OK) {
      continue;
    }
    std::unique_ptr<FileSystemFileUtil::AbstractFileEnumerator> enumerator(
        obfuscated_file_util()->CreateFileEnumerator(
            operation_context.get(),
            context->CrackURL(
                GURL(root), blink::StorageKey(url::Origin::Create(GURL(root)))),
            true));

    base::FilePath cdm_file_path;
    while (!(cdm_file_path = enumerator->Next()).empty()) {
      cdm_files.emplace_back(cdm_file_path.BaseName().AsUTF8Unsafe(),
                             plugin_path.BaseName().AsUTF8Unsafe());
    }
  }

  return cdm_files;
}

scoped_refptr<QuotaReservation>
PluginPrivateFileSystemBackend::CreateQuotaReservationOnFileTaskRunner(
    const blink::StorageKey& storage_key,
    FileSystemType type) {
  // We don't track usage on this filesystem.
  NOTREACHED();
  return scoped_refptr<QuotaReservation>();
}

const UpdateObserverList* PluginPrivateFileSystemBackend::GetUpdateObservers(
    FileSystemType type) const {
  return nullptr;
}

const ChangeObserverList* PluginPrivateFileSystemBackend::GetChangeObservers(
    FileSystemType type) const {
  return nullptr;
}

const AccessObserverList* PluginPrivateFileSystemBackend::GetAccessObservers(
    FileSystemType type) const {
  return nullptr;
}

ObfuscatedFileUtil* PluginPrivateFileSystemBackend::obfuscated_file_util() {
  return static_cast<ObfuscatedFileUtil*>(
      static_cast<AsyncFileUtilAdapter*>(file_util_.get())->sync_file_util());
}

ObfuscatedFileUtilMemoryDelegate*
PluginPrivateFileSystemBackend::obfuscated_file_util_memory_delegate() {
  auto* file_util = obfuscated_file_util();
  DCHECK(file_util->is_incognito());
  return static_cast<ObfuscatedFileUtilMemoryDelegate*>(file_util->delegate());
}

}  // namespace storage

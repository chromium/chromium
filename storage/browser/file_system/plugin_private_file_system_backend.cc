// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/plugin_private_file_system_backend.h"

#include <stdint.h>

#include <map>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/stl_util.h"
#include "base/synchronization/lock.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/url_util.h"
#include "storage/browser/file_system/async_file_util_adapter.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "storage/browser/file_system/file_stream_writer.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation.h"
#include "storage/browser/file_system/file_system_operation_context.h"
#include "storage/browser/file_system/isolated_context.h"
#include "storage/browser/file_system/obfuscated_file_util.h"
#include "storage/browser/file_system/obfuscated_file_util_memory_delegate.h"
#include "storage/browser/file_system/quota/quota_reservation.h"
#include "storage/browser/file_system/sandbox_file_stream_writer.h"
#include "storage/common/file_system/file_system_util.h"

namespace storage {

class PluginPrivateFileSystemBackend::FileSystemIDToPluginMap {
 public:
  explicit FileSystemIDToPluginMap(base::SequencedTaskRunner* task_runner)
      : task_runner_(task_runner) {}
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
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
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
    const GURL& origin_url,
    const std::string& filesystem_id,
    const std::string& plugin_id,
    OpenFileSystemMode mode) {
  base::File::Error error = base::File::FILE_ERROR_FAILED;
  const bool create = (mode == OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT);
  file_util->GetDirectoryForOriginAndType(origin_url, plugin_id, create,
                                          &error);
  if (error == base::File::FILE_OK)
    plugin_map->RegisterFileSystem(filesystem_id, plugin_id);
  return error;
}

}  // namespace

PluginPrivateFileSystemBackend::PluginPrivateFileSystemBackend(
    base::SequencedTaskRunner* file_task_runner,
    const base::FilePath& profile_path,
    storage::SpecialStoragePolicy* special_storage_policy,
    const FileSystemOptions& file_system_options,
    leveldb::Env* env_override)
    : file_task_runner_(file_task_runner),
      file_system_options_(file_system_options),
      base_path_(profile_path.Append(kFileSystemDirectory)
                     .Append(kPluginPrivateDirectory)),
      plugin_map_(new FileSystemIDToPluginMap(file_task_runner)) {
  file_util_ = std::make_unique<AsyncFileUtilAdapter>(new ObfuscatedFileUtil(
      special_storage_policy, base_path_, env_override,
      base::BindRepeating(&FileSystemIDToPluginMap::GetPluginIDForURL,
                          base::Owned(plugin_map_)),
      std::set<std::string>(), nullptr, file_system_options.is_incognito()));
}

PluginPrivateFileSystemBackend::~PluginPrivateFileSystemBackend() {
  if (!file_task_runner_->RunsTasksInCurrentSequence()) {
    AsyncFileUtil* file_util = file_util_.release();
    if (!file_task_runner_->DeleteSoon(FROM_HERE, file_util))
      delete file_util;
  }
}

void PluginPrivateFileSystemBackend::OpenPrivateFileSystem(
    const GURL& origin_url,
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
                     plugin_map_, origin_url, filesystem_id, plugin_id, mode),
      std::move(callback));
}

bool PluginPrivateFileSystemBackend::CanHandleType(FileSystemType type) const {
  return type == kFileSystemTypePluginPrivate;
}

void PluginPrivateFileSystemBackend::Initialize(FileSystemContext* context) {}

void PluginPrivateFileSystemBackend::ResolveURL(
    const FileSystemURL& url,
    OpenFileSystemMode mode,
    OpenFileSystemCallback callback) {
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

FileSystemOperation* PluginPrivateFileSystemBackend::CreateFileSystemOperation(
    const FileSystemURL& url,
    FileSystemContext* context,
    base::File::Error* error_code) const {
  std::unique_ptr<FileSystemOperationContext> operation_context(
      new FileSystemOperationContext(context));
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
  return FileStreamReader::CreateForFileSystemFile(context, url, offset,
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
PluginPrivateFileSystemBackend::DeleteOriginDataOnFileTaskRunner(
    FileSystemContext* context,
    storage::QuotaManagerProxy* proxy,
    const GURL& origin_url,
    FileSystemType type) {
  if (!CanHandleType(type))
    return base::File::FILE_ERROR_SECURITY;
  bool result = obfuscated_file_util()->DeleteDirectoryForOriginAndType(
      origin_url, std::string());
  if (result)
    return base::File::FILE_OK;
  return base::File::FILE_ERROR_FAILED;
}

void PluginPrivateFileSystemBackend::PerformStorageCleanupOnFileTaskRunner(
    FileSystemContext* context,
    storage::QuotaManagerProxy* proxy,
    FileSystemType type) {
  if (!CanHandleType(type))
    return;
  obfuscated_file_util()->RewriteDatabases();
}

void PluginPrivateFileSystemBackend::GetOriginsForTypeOnFileTaskRunner(
    FileSystemType type,
    std::set<GURL>* origins) {
  if (!CanHandleType(type))
    return;
  std::unique_ptr<ObfuscatedFileUtil::AbstractOriginEnumerator> enumerator(
      obfuscated_file_util()->CreateOriginEnumerator());
  GURL origin;
  while (!(origin = enumerator->Next()).is_empty())
    origins->insert(origin);
}

void PluginPrivateFileSystemBackend::GetOriginsForHostOnFileTaskRunner(
    FileSystemType type,
    const std::string& host,
    std::set<GURL>* origins) {
  if (!CanHandleType(type))
    return;
  std::unique_ptr<ObfuscatedFileUtil::AbstractOriginEnumerator> enumerator(
      obfuscated_file_util()->CreateOriginEnumerator());
  GURL origin;
  while (!(origin = enumerator->Next()).is_empty()) {
    if (host == net::GetHostOrSpecFromURL(origin))
      origins->insert(origin);
  }
}

int64_t PluginPrivateFileSystemBackend::GetOriginUsageOnFileTaskRunner(
    FileSystemContext* context,
    const GURL& origin_url,
    FileSystemType type) {
  DCHECK(file_task_runner_->RunsTasksInCurrentSequence());

  if (!CanHandleType(type))
    return 0;

  int64_t total_size;
  base::Time last_modified_time;
  GetOriginDetailsOnFileTaskRunner(context, origin_url, &total_size,
                                   &last_modified_time);
  return total_size;
}

void PluginPrivateFileSystemBackend::GetOriginDetailsOnFileTaskRunner(
    FileSystemContext* context,
    const GURL& origin_url,
    int64_t* total_size,
    base::Time* last_modified_time) {
  DCHECK(file_task_runner_->RunsTasksInCurrentSequence());

  *total_size = 0;
  *last_modified_time = base::Time::UnixEpoch();
  std::string fsid =
      storage::IsolatedContext::GetInstance()->RegisterFileSystemForVirtualPath(
          storage::kFileSystemTypePluginPrivate, "pluginprivate",
          base::FilePath());
  DCHECK(storage::ValidateIsolatedFileSystemId(fsid));

  std::string root = storage::GetIsolatedFileSystemRootURIString(
      origin_url, fsid, "pluginprivate");

  std::unique_ptr<FileSystemOperationContext> operation_context(
      new FileSystemOperationContext(context));

  // Determine the available plugin private filesystem directories for this
  // origin. Currently the plugin private filesystem is only used by Encrypted
  // Media Content Decryption Modules. Each CDM gets a directory based on the
  // mimetype (e.g. plugin application/x-ppapi-widevine-cdm uses directory
  // application_x-ppapi-widevine-cdm). Enumerate through the set of
  // directories so that data from any CDM used by this origin is counted.
  base::File::Error error;
  base::FilePath path = obfuscated_file_util()->GetDirectoryForOriginAndType(
      origin_url, "", false, &error);
  if (error != base::File::FILE_OK) {
    DLOG(ERROR) << "Unable to read directory for " << origin_url;
    return;
  }

  base::FileEnumerator directory_enumerator(path, false,
                                            base::FileEnumerator::DIRECTORIES);
  base::FilePath plugin_path;
  while (!(plugin_path = directory_enumerator.Next()).empty()) {
    std::string plugin_name = plugin_path.BaseName().MaybeAsASCII();
    if (OpenFileSystemOnFileTaskRunner(
            obfuscated_file_util(), plugin_map_, origin_url, fsid, plugin_name,
            storage::OPEN_FILE_SYSTEM_FAIL_IF_NONEXISTENT) !=
        base::File::FILE_OK) {
      continue;
    }

    std::unique_ptr<FileSystemFileUtil::AbstractFileEnumerator> enumerator(
        obfuscated_file_util()->CreateFileEnumerator(
            operation_context.get(), context->CrackURL(GURL(root)), true));

    while (!enumerator->Next().empty()) {
      *total_size += enumerator->Size();
      if (enumerator->LastModifiedTime() > *last_modified_time)
        *last_modified_time = enumerator->LastModifiedTime();
    }
  }
}

scoped_refptr<QuotaReservation>
PluginPrivateFileSystemBackend::CreateQuotaReservationOnFileTaskRunner(
    const GURL& origin_url,
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

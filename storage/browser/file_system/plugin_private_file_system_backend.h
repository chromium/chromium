// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_PLUGIN_PRIVATE_FILE_SYSTEM_BACKEND_H_
#define STORAGE_BROWSER_FILE_SYSTEM_PLUGIN_PRIVATE_FILE_SYSTEM_BACKEND_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "storage/browser/file_system/file_system_backend.h"
#include "storage/browser/file_system/file_system_options.h"
#include "storage/browser/file_system/file_system_quota_util.h"
#include "storage/browser/file_system/task_runner_bound_observer_list.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace blink {
class StorageKey;
}  // namespace blink

namespace leveldb {
class Env;
}  // namespace leveldb

namespace url {
class Origin;
}  // namespace url

namespace storage {

class ObfuscatedFileUtil;
class ObfuscatedFileUtilMemoryDelegate;
class SpecialStoragePolicy;
class WatcherManager;

// TODO(crbug.com/1231162): Remove this when removing the plugin private FS.
// Name of the root directory in the plugin private file system.
const char kPluginPrivateRootName[] = "pluginprivate";

class COMPONENT_EXPORT(STORAGE_BROWSER) PluginPrivateFileSystemBackend
    : public FileSystemBackend,
      public FileSystemQuotaUtil {
 public:
  class FileSystemIDToPluginMap;
  using StatusCallback = base::OnceCallback<void(base::File::Error result)>;

  // Used to migrate media license data to the new backend.
  struct COMPONENT_EXPORT(STORAGE_BROWSER) CdmFileInfo {
    CdmFileInfo(const std::string& name,
                const std::string& legacy_file_system_id);
    CdmFileInfo(const CdmFileInfo&);
    CdmFileInfo(CdmFileInfo&&);
    ~CdmFileInfo();

    const std::string name;
    const std::string legacy_file_system_id;
  };

  PluginPrivateFileSystemBackend(
      scoped_refptr<base::SequencedTaskRunner> file_task_runner,
      const base::FilePath& profile_path,
      scoped_refptr<SpecialStoragePolicy> special_storage_policy,
      const FileSystemOptions& file_system_options,
      leveldb::Env* env_override);

  PluginPrivateFileSystemBackend(const PluginPrivateFileSystemBackend&) =
      delete;
  PluginPrivateFileSystemBackend& operator=(
      const PluginPrivateFileSystemBackend&) = delete;

  ~PluginPrivateFileSystemBackend() override;

  // This must be used to open 'private' filesystem instead of regular
  // OpenFileSystem.
  // |plugin_id| must be an identifier string for per-plugin
  // isolation, e.g. name, MIME type etc.
  // NOTE: |plugin_id| must be sanitized ASCII string that doesn't
  // include *any* dangerous character like '/'.
  void OpenPrivateFileSystem(const url::Origin& origin,
                             FileSystemType type,
                             const std::string& filesystem_id,
                             const std::string& plugin_id,
                             OpenFileSystemMode mode,
                             StatusCallback callback);

  // FileSystemBackend overrides.
  bool CanHandleType(FileSystemType type) const override;
  void Initialize(FileSystemContext* context) override;
  void ResolveURL(const FileSystemURL& url,
                  OpenFileSystemMode mode,
                  ResolveURLCallback callback) override;
  AsyncFileUtil* GetAsyncFileUtil(FileSystemType type) override;
  WatcherManager* GetWatcherManager(FileSystemType type) override;
  CopyOrMoveFileValidatorFactory* GetCopyOrMoveFileValidatorFactory(
      FileSystemType type,
      base::File::Error* error_code) override;
  std::unique_ptr<FileSystemOperation> CreateFileSystemOperation(
      const FileSystemURL& url,
      FileSystemContext* context,
      base::File::Error* error_code) const override;
  bool SupportsStreaming(const FileSystemURL& url) const override;
  bool HasInplaceCopyImplementation(FileSystemType type) const override;
  std::unique_ptr<FileStreamReader> CreateFileStreamReader(
      const FileSystemURL& url,
      int64_t offset,
      int64_t max_bytes_to_read,
      const base::Time& expected_modification_time,
      FileSystemContext* context) const override;
  std::unique_ptr<FileStreamWriter> CreateFileStreamWriter(
      const FileSystemURL& url,
      int64_t offset,
      FileSystemContext* context) const override;
  FileSystemQuotaUtil* GetQuotaUtil() override;
  const UpdateObserverList* GetUpdateObservers(
      FileSystemType type) const override;
  const ChangeObserverList* GetChangeObservers(
      FileSystemType type) const override;
  const AccessObserverList* GetAccessObservers(
      FileSystemType type) const override;

  // FileSystemQuotaUtil overrides.
  base::File::Error DeleteStorageKeyDataOnFileTaskRunner(
      FileSystemContext* context,
      QuotaManagerProxy* proxy,
      const blink::StorageKey& storage_key,
      FileSystemType type) override;
  void PerformStorageCleanupOnFileTaskRunner(FileSystemContext* context,
                                             QuotaManagerProxy* proxy,
                                             FileSystemType type) override;
  std::vector<blink::StorageKey> GetStorageKeysForTypeOnFileTaskRunner(
      FileSystemType type) override;
  int64_t GetStorageKeyUsageOnFileTaskRunner(
      FileSystemContext* context,
      const blink::StorageKey& storage_key,
      FileSystemType type) override;
  scoped_refptr<QuotaReservation> CreateQuotaReservationOnFileTaskRunner(
      const blink::StorageKey& storage_key,
      FileSystemType type) override;

  // Get details on the files saved for the specified |origin_url|. Returns
  // the total size and last modified time for the set of all files stored
  // for the particular origin. |total_size| = 0 and |last_modified_time| =
  // base::Time::UnixEpoch() if no files found.
  void GetOriginDetailsOnFileTaskRunner(FileSystemContext* context,
                                        const url::Origin& origin,
                                        int64_t* total_size,
                                        base::Time* last_modified_time);

  // Used to migrate media license data to the new backend.
  // TODO(crbug.com/1231162): Once all media license data has been migrated, the
  // PPFS will have no more consumers and we can remove it entirely.
  std::vector<CdmFileInfo> GetMediaLicenseFilesForOriginOnFileTaskRunner(
      FileSystemContext* context,
      const url::Origin& origin);

  ObfuscatedFileUtilMemoryDelegate* obfuscated_file_util_memory_delegate();
  const base::FilePath& base_path() const { return base_path_; }

 private:
  friend class PluginPrivateFileSystemBackendTest;

  ObfuscatedFileUtil* obfuscated_file_util();

  const scoped_refptr<base::SequencedTaskRunner> file_task_runner_;
  const FileSystemOptions file_system_options_;
  const base::FilePath base_path_;
  std::unique_ptr<AsyncFileUtil> file_util_;
  raw_ptr<FileSystemIDToPluginMap> plugin_map_;  // Owned by file_util_.
  base::WeakPtrFactory<PluginPrivateFileSystemBackend> weak_factory_{this};
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_PLUGIN_PRIVATE_FILE_SYSTEM_BACKEND_H_

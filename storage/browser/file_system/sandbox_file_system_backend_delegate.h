// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_SANDBOX_FILE_SYSTEM_BACKEND_DELEGATE_H_
#define STORAGE_BROWSER_FILE_SYSTEM_SANDBOX_FILE_SYSTEM_BACKEND_DELEGATE_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/files/file_error_or.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "storage/browser/file_system/file_system_backend.h"
#include "storage/browser/file_system/file_system_options.h"
#include "storage/browser/file_system/file_system_quota_util.h"
#include "storage/browser/file_system/task_runner_bound_observer_list.h"
#include "storage/browser/quota/quota_manager_proxy.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace blink {
class StorageKey;
}  // namespace blink

namespace storage {
class SandboxFileSystemBackendDelegateTest;
class SandboxFileSystemTestHelper;
}  // namespace storage

namespace leveldb {
class Env;
}  // namespace leveldb

namespace url {
class Origin;
}  // namespace url

namespace storage {

class AsyncFileUtil;
class FileStreamWriter;
class FileSystemFileUtil;
class FileSystemOperationContext;
class FileStreamReader;
class FileSystemURL;
class FileSystemUsageCache;
class ObfuscatedFileUtil;
class ObfuscatedFileUtilMemoryDelegate;
class QuotaManagerProxy;
class QuotaReservationManager;
class SandboxQuotaObserver;
class SpecialStoragePolicy;

// Delegate implementation of the some methods in Sandbox/SyncFileSystemBackend.
// An instance of this class is created and owned by FileSystemContext.
class COMPONENT_EXPORT(STORAGE_BROWSER) SandboxFileSystemBackendDelegate
    : public FileSystemQuotaUtil {
 public:
  using OpenFileSystemCallback = FileSystemBackend::OpenFileSystemCallback;
  using ResolveURLCallback = FileSystemBackend::ResolveURLCallback;

  // StorageKey enumerator interface.
  // An instance of this interface is assumed to be called on the file thread.
  class StorageKeyEnumerator {
   public:
    StorageKeyEnumerator(const StorageKeyEnumerator&) = delete;
    StorageKeyEnumerator& operator=(const StorageKeyEnumerator&) = delete;
    virtual ~StorageKeyEnumerator() = default;

    // Returns the next StorageKey.  Returns std::nullopt if there are no more
    // StorageKey.
    virtual std::optional<blink::StorageKey> Next() = 0;

    // Returns the current StorageKey's information.
    virtual bool HasFileSystemType(FileSystemType type) const = 0;

   protected:
    StorageKeyEnumerator() = default;
  };

  // Returns the type directory name in sandbox directory for given |type|.
  static std::string GetTypeString(FileSystemType type);

  SandboxFileSystemBackendDelegate(
      scoped_refptr<QuotaManagerProxy> quota_manager_proxy,
      scoped_refptr<base::SequencedTaskRunner> file_task_runner,
      const base::FilePath& profile_path,
      scoped_refptr<SpecialStoragePolicy> special_storage_policy,
      const FileSystemOptions& file_system_options,
      leveldb::Env* env_override);

  SandboxFileSystemBackendDelegate(const SandboxFileSystemBackendDelegate&) =
      delete;
  SandboxFileSystemBackendDelegate& operator=(
      const SandboxFileSystemBackendDelegate&) = delete;
  ~SandboxFileSystemBackendDelegate() override;

  // Returns a StorageKey enumerator of sandbox filesystem.
  // This method can only be called on the file thread.
  StorageKeyEnumerator* CreateStorageKeyEnumerator();

  // Gets a base directory path of the sandboxed filesystem that is
  // specified by `storage_key` and `type`.
  // (The path is similar to the origin's root path but doesn't contain
  // the 'unique' part.)
  // Returns an empty path if the given type is invalid.
  // This method can only be called on the file thread.
  base::FilePath GetBaseDirectoryForStorageKeyAndType(
      const blink::StorageKey& storage_key,
      FileSystemType type,
      bool create);

  // Gets a base directory path of the sandboxed filesystem that is specified by
  // `bucket_locator` and `type`. Returns an empty path if invalid or directory
  // does not exist when `create` is false.
  base::FilePath GetBaseDirectoryForBucketAndType(
      const BucketLocator& bucket_locator,
      FileSystemType type,
      bool create);

  // FileSystemBackend helpers.
  void OpenFileSystem(const BucketLocator& bucket_locator,
                      FileSystemType type,
                      OpenFileSystemMode mode,
                      ResolveURLCallback callback,
                      const GURL& root_url);
  std::unique_ptr<FileSystemOperationContext> CreateFileSystemOperationContext(
      const FileSystemURL& url,
      FileSystemContext* context,
      base::File::Error* error_code) const;
  std::unique_ptr<FileStreamReader> CreateFileStreamReader(
      const FileSystemURL& url,
      int64_t offset,
      const base::Time& expected_modification_time,
      FileSystemContext* context) const;
  std::unique_ptr<FileStreamWriter> CreateFileStreamWriter(
      const FileSystemURL& url,
      int64_t offset,
      FileSystemContext* context,
      FileSystemType type) const;

  // FileSystemQuotaUtil overrides.
  void DeleteCachedDefaultBucket(const blink::StorageKey& storage_key) override;
  base::File::Error DeleteBucketDataOnFileTaskRunner(
      FileSystemContext* context,
      QuotaManagerProxy* proxy,
      const BucketLocator& bucket_locator,
      FileSystemType type) override;
  void PerformStorageCleanupOnFileTaskRunner(FileSystemContext* context,
                                             QuotaManagerProxy* proxy,
                                             FileSystemType type) override;
  std::vector<blink::StorageKey> GetStorageKeysForTypeOnFileTaskRunner(
      FileSystemType type) override;
  int64_t GetBucketUsageOnFileTaskRunner(FileSystemContext* context,
                                         const BucketLocator& bucket_locator,
                                         FileSystemType type) override;
  scoped_refptr<QuotaReservation> CreateQuotaReservationOnFileTaskRunner(
      const blink::StorageKey& storage_key,
      FileSystemType type) override;

  // Adds an observer for the secified |type| of a file system, bound to
  // |task_runner|.
  virtual void AddFileUpdateObserver(FileSystemType type,
                                     FileUpdateObserver* observer,
                                     base::SequencedTaskRunner* task_runner);
  virtual void AddFileChangeObserver(FileSystemType type,
                                     FileChangeObserver* observer,
                                     base::SequencedTaskRunner* task_runner);
  virtual void AddFileAccessObserver(FileSystemType type,
                                     FileAccessObserver* observer,
                                     base::SequencedTaskRunner* task_runner);

  // Returns observer lists for the specified |type| of a file system.
  virtual const UpdateObserverList* GetUpdateObservers(
      FileSystemType type) const;
  virtual const ChangeObserverList* GetChangeObservers(
      FileSystemType type) const;
  virtual const AccessObserverList* GetAccessObservers(
      FileSystemType type) const;

  // Registers quota observer for file updates on filesystem of |type|.
  void RegisterQuotaUpdateObserver(FileSystemType type);

  void InvalidateUsageCache(const blink::StorageKey& storage_key,
                            FileSystemType type);
  void StickyInvalidateUsageCache(const blink::StorageKey& storage_key,
                                  FileSystemType type);

  void CollectOpenFileSystemMetrics(base::File::Error error_code);

  base::SequencedTaskRunner* file_task_runner() {
    return file_task_runner_.get();
  }

  AsyncFileUtil* file_util() { return sandbox_file_util_.get(); }
  FileSystemUsageCache* usage_cache() { return file_system_usage_cache_.get(); }
  SandboxQuotaObserver* quota_observer() { return quota_observer_.get(); }

  SpecialStoragePolicy* special_storage_policy() {
    return special_storage_policy_.get();
  }

  const FileSystemOptions& file_system_options() const {
    return file_system_options_;
  }

  const scoped_refptr<QuotaManagerProxy> quota_manager_proxy() const {
    return quota_manager_proxy_;
  }

  FileSystemFileUtil* sync_file_util();

  base::WeakPtr<ObfuscatedFileUtilMemoryDelegate> memory_file_util_delegate();

 private:
  friend class QuotaBackendImpl;
  friend class SandboxQuotaObserver;
  friend class SandboxFileSystemBackendDelegateTest;
  friend class SandboxFileSystemTestHelper;

  // Performs API-specific validity checks on the given path |url|.
  // Returns true if access to |url| is valid in this filesystem.
  bool IsAccessValid(const FileSystemURL& url) const;

  // Returns true if the given |url|'s scheme is allowed to access
  // filesystem.
  bool IsAllowedScheme(const GURL& url) const;

  // Returns a path to the usage cache file.
  base::FileErrorOr<base::FilePath> GetUsageCachePathForStorageKeyAndType(
      const blink::StorageKey& storage_key,
      FileSystemType type);

  // Returns a path to the usage cache file (static version).
  static base::FileErrorOr<base::FilePath>
  GetUsageCachePathForStorageKeyAndType(ObfuscatedFileUtil* sandbox_file_util,
                                        const blink::StorageKey& storage_key,
                                        FileSystemType type);

  // Returns a path to the usage cache file for a given bucket and type.
  base::FileErrorOr<base::FilePath> GetUsageCachePathForBucketAndType(
      const BucketLocator& bucket_locator,
      FileSystemType type);

  // Returns a path to the usage cache file for a given bucket and type(static
  // version).
  static base::FileErrorOr<base::FilePath> GetUsageCachePathForBucketAndType(
      ObfuscatedFileUtil* sandbox_file_util,
      const BucketLocator& bucket_locator,
      FileSystemType type);

  int64_t RecalculateBucketUsage(FileSystemContext* context,
                                 const BucketLocator& bucket_locator,
                                 FileSystemType type);

  ObfuscatedFileUtil* obfuscated_file_util();

  const scoped_refptr<base::SequencedTaskRunner> file_task_runner_;
  const scoped_refptr<QuotaManagerProxy> quota_manager_proxy_;

  std::unique_ptr<AsyncFileUtil> sandbox_file_util_;
  std::unique_ptr<FileSystemUsageCache> file_system_usage_cache_;
  std::unique_ptr<SandboxQuotaObserver> quota_observer_;
  std::unique_ptr<QuotaReservationManager> quota_reservation_manager_;

  const scoped_refptr<SpecialStoragePolicy> special_storage_policy_;

  FileSystemOptions file_system_options_;

  bool is_filesystem_opened_;
  THREAD_CHECKER(io_thread_checker_);

  // Accessed only on the file thread.
  std::set<url::Origin> visited_origins_;

  std::set<std::pair<url::Origin, FileSystemType>> sticky_dirty_origins_;

  std::map<FileSystemType, UpdateObserverList> update_observers_;
  std::map<FileSystemType, ChangeObserverList> change_observers_;
  std::map<FileSystemType, AccessObserverList> access_observers_;

  base::Time next_release_time_for_open_filesystem_stat_;

  base::WeakPtrFactory<SandboxFileSystemBackendDelegate> weak_factory_{this};
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_SANDBOX_FILE_SYSTEM_BACKEND_DELEGATE_H_

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_SANDBOX_FILE_SYSTEM_BACKEND_DELEGATE_H_
#define STORAGE_BROWSER_FILE_SYSTEM_SANDBOX_FILE_SYSTEM_BACKEND_DELEGATE_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "storage/browser/file_system/file_system_backend.h"
#include "storage/browser/file_system/file_system_options.h"
#include "storage/browser/file_system/file_system_quota_util.h"
#include "storage/browser/file_system/task_runner_bound_observer_list.h"

namespace base {
class SequencedTaskRunner;
}

namespace content {
class SandboxFileSystemBackendDelegateTest;
class SandboxFileSystemTestHelper;
}  // namespace content

namespace leveldb {
class Env;
}

namespace storage {
class QuotaManagerProxy;
class SpecialStoragePolicy;
}  // namespace storage

namespace storage {
class FileStreamReader;
}

namespace storage {

class AsyncFileUtil;
class FileStreamWriter;
class FileSystemFileUtil;
class FileSystemOperationContext;
class FileSystemURL;
class FileSystemUsageCache;
class ObfuscatedFileUtil;
class ObfuscatedFileUtilMemoryDelegate;
class QuotaReservationManager;
class SandboxQuotaObserver;

// Delegate implementation of the some methods in Sandbox/SyncFileSystemBackend.
// An instance of this class is created and owned by FileSystemContext.
class COMPONENT_EXPORT(STORAGE_BROWSER) SandboxFileSystemBackendDelegate
    : public FileSystemQuotaUtil {
 public:
  using OpenFileSystemCallback = FileSystemBackend::OpenFileSystemCallback;

  // The FileSystem directory name.
  static const base::FilePath::CharType kFileSystemDirectory[];

  // Origin enumerator interface.
  // An instance of this interface is assumed to be called on the file thread.
  class OriginEnumerator {
   public:
    virtual ~OriginEnumerator() {}

    // Returns the next origin.  Returns empty if there are no more origins.
    virtual GURL Next() = 0;

    // Returns the current origin's information.
    virtual bool HasFileSystemType(FileSystemType type) const = 0;
  };

  // Returns the type directory name in sandbox directory for given |type|.
  static std::string GetTypeString(FileSystemType type);

  SandboxFileSystemBackendDelegate(
      storage::QuotaManagerProxy* quota_manager_proxy,
      base::SequencedTaskRunner* file_task_runner,
      const base::FilePath& profile_path,
      storage::SpecialStoragePolicy* special_storage_policy,
      const FileSystemOptions& file_system_options,
      leveldb::Env* env_override);

  ~SandboxFileSystemBackendDelegate() override;

  // Returns an origin enumerator of sandbox filesystem.
  // This method can only be called on the file thread.
  OriginEnumerator* CreateOriginEnumerator();

  // Gets a base directory path of the sandboxed filesystem that is
  // specified by |origin_url| and |type|.
  // (The path is similar to the origin's root path but doesn't contain
  // the 'unique' part.)
  // Returns an empty path if the given type is invalid.
  // This method can only be called on the file thread.
  base::FilePath GetBaseDirectoryForOriginAndType(const GURL& origin_url,
                                                  FileSystemType type,
                                                  bool create);

  // FileSystemBackend helpers.
  void OpenFileSystem(const GURL& origin_url,
                      FileSystemType type,
                      OpenFileSystemMode mode,
                      OpenFileSystemCallback callback,
                      const GURL& root_url);
  std::unique_ptr<FileSystemOperationContext> CreateFileSystemOperationContext(
      const FileSystemURL& url,
      FileSystemContext* context,
      base::File::Error* error_code) const;
  std::unique_ptr<storage::FileStreamReader> CreateFileStreamReader(
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
  base::File::Error DeleteOriginDataOnFileTaskRunner(
      FileSystemContext* context,
      storage::QuotaManagerProxy* proxy,
      const GURL& origin_url,
      FileSystemType type) override;
  void PerformStorageCleanupOnFileTaskRunner(FileSystemContext* context,
                                             storage::QuotaManagerProxy* proxy,
                                             FileSystemType type) override;
  void GetOriginsForTypeOnFileTaskRunner(FileSystemType type,
                                         std::set<GURL>* origins) override;
  void GetOriginsForHostOnFileTaskRunner(FileSystemType type,
                                         const std::string& host,
                                         std::set<GURL>* origins) override;
  int64_t GetOriginUsageOnFileTaskRunner(FileSystemContext* context,
                                         const GURL& origin_url,
                                         FileSystemType type) override;
  scoped_refptr<QuotaReservation> CreateQuotaReservationOnFileTaskRunner(
      const GURL& origin_url,
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

  void InvalidateUsageCache(const GURL& origin_url, FileSystemType type);
  void StickyInvalidateUsageCache(const GURL& origin_url, FileSystemType type);

  void CollectOpenFileSystemMetrics(base::File::Error error_code);

  // Used for migrating from the general storage partition to an isolated
  // storage partition
  void CopyFileSystem(const GURL& origin_url,
                      FileSystemType type,
                      SandboxFileSystemBackendDelegate* destination);

  base::SequencedTaskRunner* file_task_runner() {
    return file_task_runner_.get();
  }

  AsyncFileUtil* file_util() { return sandbox_file_util_.get(); }
  FileSystemUsageCache* usage_cache() { return file_system_usage_cache_.get(); }
  SandboxQuotaObserver* quota_observer() { return quota_observer_.get(); }

  storage::SpecialStoragePolicy* special_storage_policy() {
    return special_storage_policy_.get();
  }

  const FileSystemOptions& file_system_options() const {
    return file_system_options_;
  }

  FileSystemFileUtil* sync_file_util();

  base::WeakPtr<ObfuscatedFileUtilMemoryDelegate> memory_file_util_delegate();

 private:
  friend class QuotaBackendImpl;
  friend class SandboxQuotaObserver;
  friend class content::SandboxFileSystemBackendDelegateTest;
  friend class content::SandboxFileSystemTestHelper;

  // Performs API-specific validity checks on the given path |url|.
  // Returns true if access to |url| is valid in this filesystem.
  bool IsAccessValid(const FileSystemURL& url) const;

  // Returns true if the given |url|'s scheme is allowed to access
  // filesystem.
  bool IsAllowedScheme(const GURL& url) const;

  // Returns a path to the usage cache file.
  base::FilePath GetUsageCachePathForOriginAndType(const GURL& origin_url,
                                                   FileSystemType type);

  // Returns a path to the usage cache file (static version).
  static base::FilePath GetUsageCachePathForOriginAndType(
      ObfuscatedFileUtil* sandbox_file_util,
      const GURL& origin_url,
      FileSystemType type,
      base::File::Error* error_out);

  int64_t RecalculateUsage(FileSystemContext* context,
                           const GURL& origin,
                           FileSystemType type);

  ObfuscatedFileUtil* obfuscated_file_util();

  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;
  scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy_;

  std::unique_ptr<AsyncFileUtil> sandbox_file_util_;
  std::unique_ptr<FileSystemUsageCache> file_system_usage_cache_;
  std::unique_ptr<SandboxQuotaObserver> quota_observer_;
  std::unique_ptr<QuotaReservationManager> quota_reservation_manager_;

  scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy_;

  FileSystemOptions file_system_options_;

  bool is_filesystem_opened_;
  THREAD_CHECKER(io_thread_checker_);

  // Accessed only on the file thread.
  std::set<GURL> visited_origins_;

  std::set<std::pair<GURL, FileSystemType>> sticky_dirty_origins_;

  std::map<FileSystemType, UpdateObserverList> update_observers_;
  std::map<FileSystemType, ChangeObserverList> change_observers_;
  std::map<FileSystemType, AccessObserverList> access_observers_;

  base::Time next_release_time_for_open_filesystem_stat_;

  base::WeakPtrFactory<SandboxFileSystemBackendDelegate> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SandboxFileSystemBackendDelegate);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_SANDBOX_FILE_SYSTEM_BACKEND_DELEGATE_H_

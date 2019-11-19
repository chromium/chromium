// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_LEGACY_LEGACY_CACHE_STORAGE_MANAGER_H_
#define CONTENT_BROWSER_CACHE_STORAGE_LEGACY_LEGACY_CACHE_STORAGE_MANAGER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "content/browser/cache_storage/cache_storage_context_impl.h"
#include "content/browser/cache_storage/cache_storage_manager.h"
#include "content/browser/cache_storage/legacy/legacy_cache_storage.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cache_storage_context.h"
#include "content/public/browser/storage_usage_info.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "storage/browser/quota/quota_client.h"
#include "url/origin.h"

namespace base {
class SequencedTaskRunner;
}

namespace content {

namespace cache_storage_manager_unittest {
class CacheStorageManagerTest;
}

// A concrete implementation of the CacheStorageManager interface using
// the legacy disk_cache backend.
class CONTENT_EXPORT LegacyCacheStorageManager : public CacheStorageManager {
 public:
  static scoped_refptr<LegacyCacheStorageManager> Create(
      const base::FilePath& path,
      scoped_refptr<base::SequencedTaskRunner> cache_task_runner,
      scoped_refptr<base::SequencedTaskRunner> scheduler_task_runner,
      scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
      scoped_refptr<CacheStorageContextImpl::ObserverList> observers);

  // Create a new manager using the underlying configuration of the given
  // manager, but with its own list of storage objects.  This is only used
  // for testing.
  static scoped_refptr<LegacyCacheStorageManager> CreateForTesting(
      LegacyCacheStorageManager* old_manager);

  // Map a database identifier (computed from an origin) to the path.
  static base::FilePath ConstructOriginPath(const base::FilePath& root_path,
                                            const url::Origin& origin,
                                            CacheStorageOwner owner);

  // Open the CacheStorage for the given origin and owner.  A reference counting
  // handle is returned which can be stored and used similar to a weak pointer.
  CacheStorageHandle OpenCacheStorage(const url::Origin& origin,
                                      CacheStorageOwner owner) override;

  void GetAllOriginsUsage(
      CacheStorageOwner owner,
      CacheStorageContext::GetUsageInfoCallback callback) override;
  void GetOriginUsage(const url::Origin& origin_url,
                      CacheStorageOwner owner,
                      storage::QuotaClient::GetUsageCallback callback) override;
  void GetOrigins(CacheStorageOwner owner,
                  storage::QuotaClient::GetOriginsCallback callback) override;
  void GetOriginsForHost(
      const std::string& host,
      CacheStorageOwner owner,
      storage::QuotaClient::GetOriginsCallback callback) override;
  void DeleteOriginData(
      const url::Origin& origin,
      CacheStorageOwner owner,
      storage::QuotaClient::DeletionCallback callback) override;
  void DeleteOriginData(const url::Origin& origin,
                        CacheStorageOwner owner) override;

  void SetBlobParametersForCache(
      scoped_refptr<BlobStorageContextWrapper> blob_storage_context) override;

  void NotifyCacheListChanged(const url::Origin& origin);
  void NotifyCacheContentChanged(const url::Origin& origin,
                                 const std::string& name);

  base::FilePath root_path() const { return root_path_; }

  // This method is called when the last CacheStorageHandle for a particular
  // instance is destroyed and its reference count drops to zero.
  void CacheStorageUnreferenced(LegacyCacheStorage* cache_storage,
                                const url::Origin& origin,
                                CacheStorageOwner owner);

 private:
  friend class cache_storage_manager_unittest::CacheStorageManagerTest;
  friend class CacheStorageContextImpl;

  typedef std::map<std::pair<url::Origin, CacheStorageOwner>,
                   std::unique_ptr<LegacyCacheStorage>>
      CacheStorageMap;

  LegacyCacheStorageManager(
      const base::FilePath& path,
      scoped_refptr<base::SequencedTaskRunner> cache_task_runner,
      scoped_refptr<base::SequencedTaskRunner> scheduler_task_runner,
      scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
      scoped_refptr<CacheStorageContextImpl::ObserverList> observers);

  ~LegacyCacheStorageManager() override;

  void GetAllOriginsUsageGetSizes(
      std::unique_ptr<std::vector<StorageUsageInfo>> usage_info,
      CacheStorageContext::GetUsageInfoCallback callback);

  void DeleteOriginDidClose(const url::Origin& origin,
                            CacheStorageOwner owner,
                            storage::QuotaClient::DeletionCallback callback,
                            std::unique_ptr<LegacyCacheStorage> cache_storage,
                            int64_t origin_size);

  scoped_refptr<base::SequencedTaskRunner> cache_task_runner() const {
    return cache_task_runner_;
  }

  scoped_refptr<base::SequencedTaskRunner> scheduler_task_runner() const {
    return scheduler_task_runner_;
  }

  bool IsMemoryBacked() const { return root_path_.empty(); }

  // MemoryPressureListener callback
  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel level);

  base::FilePath root_path_;
  scoped_refptr<base::SequencedTaskRunner> cache_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> scheduler_task_runner_;

  scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy_;

  // The map owns the CacheStorages and the CacheStorages are only accessed on
  // |cache_task_runner_|.
  CacheStorageMap cache_storage_map_;

  scoped_refptr<CacheStorageContextImpl::ObserverList> observers_;

  scoped_refptr<BlobStorageContextWrapper> blob_storage_context_;

  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(LegacyCacheStorageManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_LEGACY_LEGACY_CACHE_STORAGE_MANAGER_H_

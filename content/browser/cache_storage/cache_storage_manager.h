// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_MANAGER_H_
#define CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_MANAGER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/dcheck_is_on.h"
#include "base/files/file_path.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/mojom/cache_storage_control.mojom.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "content/browser/cache_storage/blob_storage_context_wrapper.h"
#include "content/browser/cache_storage/cache_storage.h"
#include "content/browser/cache_storage/cache_storage_cache.h"
#include "content/browser/cache_storage/cache_storage_context_impl.h"
#include "content/browser/cache_storage/cache_storage_dispatcher_host.h"
#include "content/browser/cache_storage/cache_storage_handle.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/origin.h"

namespace base {
class SequencedTaskRunner;
}

namespace content {

namespace cache_storage_manager_unittest {
class CacheStorageManagerTest;
}

// Keeps track of a CacheStorage per StorageKey. There is one
// CacheStorageManager per CacheStorageOwner. Created and accessed from a single
// sequence.
// TODO(jkarlin): Remove CacheStorage from memory once they're no
// longer in active use.
class CONTENT_EXPORT CacheStorageManager
    : public base::RefCounted<CacheStorageManager> {
 public:
  static scoped_refptr<CacheStorageManager> Create(
      const base::FilePath& path,
      scoped_refptr<base::SequencedTaskRunner> cache_task_runner,
      scoped_refptr<base::SequencedTaskRunner> scheduler_task_runner,
      scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
      scoped_refptr<BlobStorageContextWrapper> blob_storage_context,
      base::WeakPtr<CacheStorageDispatcherHost> cache_storage_dispatcher_host);

  // Create a new manager using the underlying configuration of the given
  // manager, but with its own list of storage objects.  This is only used
  // for testing.
  static scoped_refptr<CacheStorageManager> CreateForTesting(
      CacheStorageManager* old_manager);

  CacheStorageManager(const CacheStorageManager&) = delete;
  CacheStorageManager& operator=(const CacheStorageManager&) = delete;

  // Map a database identifier (computed from a BucketLocator) to the path.
  static base::FilePath ConstructBucketPath(
      const base::FilePath& profile_path,
      const storage::BucketLocator& bucket_locator,
      storage::mojom::CacheStorageOwner owner);

  static bool IsValidQuotaStorageKey(const blink::StorageKey& storage_key);

  // Open the CacheStorage for the given bucket_locator and owner. A reference
  // counting handle is returned which can be stored and used similar to a weak
  // pointer.
  CacheStorageHandle OpenCacheStorage(
      const storage::BucketLocator& bucket_locator,
      storage::mojom::CacheStorageOwner owner);

  // QuotaClient support.
  void GetBucketUsage(
      const storage::BucketLocator& bucket_locator,
      storage::mojom::CacheStorageOwner owner,
      storage::mojom::QuotaClient::GetBucketUsageCallback callback);
  void GetStorageKeys(
      storage::mojom::CacheStorageOwner owner,
      storage::mojom::QuotaClient::GetStorageKeysForTypeCallback callback);
  void DeleteOriginData(
      const std::set<url::Origin>& origins,
      storage::mojom::CacheStorageOwner owner,
      storage::mojom::QuotaClient::DeleteBucketDataCallback callback);
  void DeleteBucketData(
      const storage::BucketLocator& bucket_locator,
      storage::mojom::CacheStorageOwner owner,
      storage::mojom::QuotaClient::DeleteBucketDataCallback callback);
  void AddObserver(
      mojo::PendingRemote<storage::mojom::CacheStorageObserver> observer);

  void NotifyCacheListChanged(const storage::BucketLocator& bucket_locator);
  void NotifyCacheContentChanged(const storage::BucketLocator& bucket_locator,
                                 const std::string& name);

  base::FilePath profile_path() const { return profile_path_; }

  static base::FilePath ConstructFirstPartyDefaultRootPath(
      const base::FilePath& profile_path);

  static base::FilePath ConstructThirdPartyAndNonDefaultRootPath(
      const base::FilePath& profile_path);

  // This method is called when the last CacheStorageHandle for a particular
  // instance is destroyed and its reference count drops to zero.
  void CacheStorageUnreferenced(CacheStorage* cache_storage,
                                const storage::BucketLocator& bucket_locator,
                                storage::mojom::CacheStorageOwner owner);

 protected:
  friend class base::RefCounted<CacheStorageManager>;

  CacheStorageManager(
      const base::FilePath& path,
      scoped_refptr<base::SequencedTaskRunner> cache_task_runner,
      scoped_refptr<base::SequencedTaskRunner> scheduler_task_runner,
      scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
      scoped_refptr<BlobStorageContextWrapper> blob_storage_context,
      base::WeakPtr<CacheStorageDispatcherHost> cache_storage_dispatcher_host);
  virtual ~CacheStorageManager();

 private:
  friend class cache_storage_manager_unittest::CacheStorageManagerTest;
  friend class CacheStorageContextImpl;

  typedef std::map<
      std::pair<storage::BucketLocator, storage::mojom::CacheStorageOwner>,
      std::unique_ptr<CacheStorage>>
      CacheStorageMap;

  void DeleteOriginsDataGotAllBucketInfo(
      const std::set<url::Origin>& origins,
      storage::mojom::CacheStorageOwner owner,
      base::OnceCallback<void(blink::mojom::QuotaStatusCode)> callback,
      std::vector<storage::BucketLocator> buckets);

  void GetBucketUsageDidGetExists(
      const storage::BucketLocator& bucket_locator,
      storage::mojom::CacheStorageOwner owner,
      storage::mojom::QuotaClient::GetBucketUsageCallback callback,
      bool exists);

  void DeleteBucketDataDidGetExists(
      storage::mojom::CacheStorageOwner owner,
      storage::mojom::QuotaClient::DeleteBucketDataCallback callback,
      storage::BucketLocator bucket_locator,
      bool exists);

  void DeleteBucketDidClose(
      const storage::BucketLocator& bucket_locator,
      storage::mojom::CacheStorageOwner owner,
      storage::mojom::QuotaClient::DeleteBucketDataCallback callback,
      std::unique_ptr<CacheStorage> cache_storage,
      int64_t bucket_size);

  scoped_refptr<base::SequencedTaskRunner> cache_task_runner() const {
    return cache_task_runner_;
  }

  scoped_refptr<base::SequencedTaskRunner> scheduler_task_runner() const {
    return scheduler_task_runner_;
  }

  void ListStorageKeysOnTaskRunner(
      storage::mojom::QuotaClient::GetStorageKeysForTypeCallback callback,
      std::vector<storage::BucketLocator> buckets);

  bool IsMemoryBacked() const { return profile_path_.empty(); }

  // MemoryPressureListener callback
  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel level);

#if DCHECK_IS_ON()
  bool CacheStoragePathIsUnique(const base::FilePath& path);
#endif
  bool ConflictingInstanceExistsInMap(
      storage::mojom::CacheStorageOwner owner,
      const storage::BucketLocator& bucket_locator);

  // Stores the storage partition (profile) path unless the CacheStorage should
  // be in-memory only, in which case this is empty.
  const base::FilePath profile_path_;
  const scoped_refptr<base::SequencedTaskRunner> cache_task_runner_;
  const scoped_refptr<base::SequencedTaskRunner> scheduler_task_runner_;

  const scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy_;

  // The map owns the CacheStorages and the CacheStorages are only accessed on
  // |cache_task_runner_|.
  CacheStorageMap cache_storage_map_;

  mojo::RemoteSet<storage::mojom::CacheStorageObserver> observers_;

  const scoped_refptr<BlobStorageContextWrapper> blob_storage_context_;

  const base::WeakPtr<CacheStorageDispatcherHost>
      cache_storage_dispatcher_host_;

  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<CacheStorageManager> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_MANAGER_H_

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_LEGACY_LEGACY_CACHE_STORAGE_H_
#define CONTENT_BROWSER_CACHE_STORAGE_LEGACY_LEGACY_CACHE_STORAGE_H_

#include <stdint.h>

#include <map>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "components/services/storage/public/mojom/blob_storage_context.mojom.h"
#include "content/browser/cache_storage/cache_storage.h"
#include "content/browser/cache_storage/cache_storage_cache_observer.h"
#include "content/browser/cache_storage/cache_storage_manager.h"
#include "content/browser/cache_storage/cache_storage_scheduler_types.h"
#include "content/browser/cache_storage/legacy/legacy_cache_storage_cache.h"
#include "mojo/public/cpp/bindings/remote.h"

#if defined(OS_ANDROID)
#include "base/android/application_status_listener.h"
#endif

namespace base {
class SequencedTaskRunner;
}

namespace content {
class CacheStorageIndex;
class CacheStorageScheduler;
enum class CacheStorageOwner;
class LegacyCacheStorageManager;

namespace cache_storage_manager_unittest {
class CacheStorageManagerTest;
FORWARD_DECLARE_TEST(CacheStorageManagerTest, PersistedCacheKeyUsed);
FORWARD_DECLARE_TEST(CacheStorageManagerTest, PutResponseWithExistingFileTest);
FORWARD_DECLARE_TEST(CacheStorageManagerTest, TestErrorInitializingCache);
}  // namespace cache_storage_manager_unittest

// TODO(jkarlin): Constrain the total bytes used per origin.

// Concrete implementation of the CacheStorage abstract class.  This is
// the legacy implementation using LegacyCacheStorageCache objects.
class CONTENT_EXPORT LegacyCacheStorage : public CacheStorage,
                                          public CacheStorageCacheObserver {
 public:
  using SizeCallback = base::OnceCallback<void(int64_t)>;

  static const char kIndexFileName[];

  LegacyCacheStorage(
      const base::FilePath& origin_path,
      bool memory_only,
      base::SequencedTaskRunner* cache_task_runner,
      scoped_refptr<base::SequencedTaskRunner> scheduler_task_runner,
      scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
      scoped_refptr<BlobStorageContextWrapper> blob_storage_context,
      LegacyCacheStorageManager* cache_storage_manager,
      const url::Origin& origin,
      CacheStorageOwner owner);

  // Any unfinished asynchronous operations may not complete or call their
  // callbacks.
  ~LegacyCacheStorage() override;

  CacheStorageHandle CreateHandle() override;

  // These methods are called by the CacheStorageHandle to track the number
  // of outstanding references.
  void AddHandleRef() override;
  void DropHandleRef() override;
  void AssertUnreferenced() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(!handle_ref_count_);
  }

  void Init() override;

  void OpenCache(const std::string& cache_name,
                 int64_t trace_id,
                 CacheAndErrorCallback callback) override;

  void HasCache(const std::string& cache_name,
                int64_t trace_id,
                BoolAndErrorCallback callback) override;

  void DoomCache(const std::string& cache_name,
                 int64_t trace_id,
                 ErrorCallback callback) override;

  void EnumerateCaches(int64_t trace_id,
                       EnumerateCachesCallback callback) override;

  void MatchCache(const std::string& cache_name,
                  blink::mojom::FetchAPIRequestPtr request,
                  blink::mojom::CacheQueryOptionsPtr match_options,
                  CacheStorageSchedulerPriority priority,
                  int64_t trace_id,
                  CacheStorageCache::ResponseCallback callback) override;

  void MatchAllCaches(blink::mojom::FetchAPIRequestPtr request,
                      blink::mojom::CacheQueryOptionsPtr match_options,
                      CacheStorageSchedulerPriority priority,
                      int64_t trace_id,
                      CacheStorageCache::ResponseCallback callback) override;

  void WriteToCache(const std::string& cache_name,
                    blink::mojom::FetchAPIRequestPtr request,
                    blink::mojom::FetchAPIResponsePtr response,
                    int64_t trace_id,
                    ErrorCallback callback) override;

  // Sums the sizes of each cache and closes them. Runs |callback| with the
  // size. The sizes include any doomed caches and will also force close all
  // caches even if there are existing handles to them.
  void GetSizeThenCloseAllCaches(SizeCallback callback);

  // The size of all of the origin's contents. This value should be used as an
  // estimate only since the cache may be modified at any time.
  void Size(SizeCallback callback);

  // The functions below are for tests to verify that the operations run
  // serially.
  CacheStorageSchedulerId StartAsyncOperationForTesting();
  void CompleteAsyncOperationForTesting(CacheStorageSchedulerId id);

  // Removes the manager reference. Called before this storage is deleted by the
  // manager, since it is removed from manager's storage map before deleting.
  void ResetManager();

  // CacheStorageCacheObserver:
  void CacheSizeUpdated(const LegacyCacheStorageCache* cache) override;

  // Destroy any CacheStorageCache instances that are not currently referenced
  // by a CacheStorageCacheHandle.
  void ReleaseUnreferencedCaches();

  static LegacyCacheStorage* From(const CacheStorageHandle& handle) {
    return static_cast<LegacyCacheStorage*>(handle.value());
  }

 protected:
  // Virtual for testing
  virtual void CacheUnreferenced(LegacyCacheStorageCache* cache);

 private:
  friend class LegacyCacheStorageCache;
  friend class cache_storage_manager_unittest::CacheStorageManagerTest;
  FRIEND_TEST_ALL_PREFIXES(
      cache_storage_manager_unittest::CacheStorageManagerTest,
      PersistedCacheKeyUsed);
  FRIEND_TEST_ALL_PREFIXES(
      cache_storage_manager_unittest::CacheStorageManagerTest,
      PutResponseWithExistingFileTest);
  FRIEND_TEST_ALL_PREFIXES(
      cache_storage_manager_unittest::CacheStorageManagerTest,
      TestErrorInitializingCache);
  class CacheLoader;
  class MemoryLoader;
  class SimpleCacheLoader;
  struct CacheMatchResponse;

  typedef std::map<std::string, std::unique_ptr<LegacyCacheStorageCache>>
      CacheMap;

  // Generate a new padding key. For testing only and *not thread safe*.
  static void GenerateNewKeyForTesting();

  // Returns a CacheStorageCacheHandle for the given name if the name is known.
  // If the CacheStorageCache has been deleted, creates a new one.
  CacheStorageCacheHandle GetLoadedCache(const std::string& cache_name);

  // Initializer and its callback are below.
  void LazyInit();
  void LazyInitImpl();
  void LazyInitDidLoadIndex(std::unique_ptr<CacheStorageIndex> index);

  // The Open and CreateCache callbacks are below.
  void OpenCacheImpl(const std::string& cache_name,
                     int64_t trace_id,
                     CacheAndErrorCallback callback);
  void CreateCacheDidCreateCache(const std::string& cache_name,
                                 int64_t trace_id,
                                 CacheAndErrorCallback callback,
                                 std::unique_ptr<LegacyCacheStorageCache> cache,
                                 blink::mojom::CacheStorageError status);
  void CreateCacheDidWriteIndex(CacheAndErrorCallback callback,
                                CacheStorageCacheHandle cache_handle,
                                int64_t trace_id,
                                bool success);

  // The HasCache callbacks are below.
  void HasCacheImpl(const std::string& cache_name,
                    int64_t trace_id,
                    BoolAndErrorCallback callback);

  // The DeleteCache callbacks are below.
  void DoomCacheImpl(const std::string& cache_name,
                     int64_t trace_id,
                     ErrorCallback callback);
  void DeleteCacheDidWriteIndex(CacheStorageCacheHandle cache_handle,
                                ErrorCallback callback,
                                int64_t trace_id,
                                bool success);
  void DeleteCacheFinalize(LegacyCacheStorageCache* doomed_cache);
  void DeleteCacheDidGetSize(LegacyCacheStorageCache* doomed_cache,
                             int64_t cache_size);
  void DeleteCacheDidCleanUp(bool success);

  // The EnumerateCache callbacks are below.
  void EnumerateCachesImpl(int64_t trace_id, EnumerateCachesCallback callback);

  // The MatchCache callbacks are below.
  void MatchCacheImpl(const std::string& cache_name,
                      blink::mojom::FetchAPIRequestPtr request,
                      blink::mojom::CacheQueryOptionsPtr match_options,
                      CacheStorageSchedulerPriority priority,
                      int64_t trace_id,
                      CacheStorageCache::ResponseCallback callback);
  void MatchCacheDidMatch(CacheStorageCacheHandle cache_handle,
                          int64_t trace_id,
                          CacheStorageCache::ResponseCallback callback,
                          blink::mojom::CacheStorageError error,
                          blink::mojom::FetchAPIResponsePtr response);

  // The MatchAllCaches callbacks are below.
  void MatchAllCachesImpl(blink::mojom::FetchAPIRequestPtr request,
                          blink::mojom::CacheQueryOptionsPtr match_options,
                          CacheStorageSchedulerPriority priority,
                          int64_t trace_id,
                          CacheStorageCache::ResponseCallback callback);
  void MatchAllCachesDidMatch(CacheStorageCacheHandle cache_handle,
                              CacheMatchResponse* out_match_response,
                              const base::RepeatingClosure& barrier_closure,
                              int64_t trace_id,
                              blink::mojom::CacheStorageError error,
                              blink::mojom::FetchAPIResponsePtr response);
  void MatchAllCachesDidMatchAll(
      std::unique_ptr<std::vector<CacheMatchResponse>> match_responses,
      int64_t trace_id,
      CacheStorageCache::ResponseCallback callback);

  // WriteToCache callbacks.
  void WriteToCacheImpl(const std::string& cache_name,
                        blink::mojom::FetchAPIRequestPtr request,
                        blink::mojom::FetchAPIResponsePtr response,
                        int64_t trace_id,
                        ErrorCallback callback);

  void GetSizeThenCloseAllCachesImpl(SizeCallback callback);

  void SizeImpl(SizeCallback callback);
  void SizeRetrievedFromCache(CacheStorageCacheHandle cache_handle,
                              base::OnceClosure closure,
                              int64_t* accumulator,
                              int64_t size);

  void NotifyCacheContentChanged(const std::string& cache_name);

  void ScheduleWriteIndex();
  void WriteIndex(base::OnceCallback<void(bool)> callback);
  void WriteIndexImpl(base::OnceCallback<void(bool)> callback);
  bool index_write_pending() const { return !index_write_task_.IsCancelled(); }
  // Start a scheduled index write immediately. Returns true if a write was
  // scheduled, or false if not.
  bool InitiateScheduledIndexWriteForTest(
      base::OnceCallback<void(bool)> callback);

  void FlushIndexIfDirty();

#if defined(OS_ANDROID)
  void OnApplicationStateChange(base::android::ApplicationState state);
#endif

  // Whether or not we've loaded the list of cache names into memory.
  bool initialized_;
  bool initializing_;

  // True if the backend is supposed to reside in memory only.
  bool memory_only_;

  // The pending operation scheduler.
  std::unique_ptr<CacheStorageScheduler> scheduler_;

  // The map of cache names to CacheStorageCache objects.
  CacheMap cache_map_;

  // Caches that have been deleted but must still be held onto until all handles
  // have been released.
  std::map<LegacyCacheStorageCache*, std::unique_ptr<LegacyCacheStorageCache>>
      doomed_caches_;

  // The cache index data.
  std::unique_ptr<CacheStorageIndex> cache_index_;

  // The file path for this CacheStorage.
  base::FilePath origin_path_;

  // The TaskRunner to run file IO on.
  scoped_refptr<base::SequencedTaskRunner> cache_task_runner_;

  // Performs backend specific operations (memory vs disk).
  std::unique_ptr<CacheLoader> cache_loader_;

  // The quota manager.
  scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy_;

  // An IO thread bound wrapper for storage.mojom.BlobStorageContext.
  scoped_refptr<BlobStorageContextWrapper> blob_storage_context_;

  // The owner that this CacheStorage is associated with.
  CacheStorageOwner owner_;

  CacheStorageSchedulerId init_id_ = -1;

  // The manager that owns this cache storage. Only set to null by
  // RemoveManager() when this cache storage is being deleted.
  LegacyCacheStorageManager* cache_storage_manager_;

  base::CancelableOnceClosure index_write_task_;
  size_t handle_ref_count_ = 0;

#if defined(OS_ANDROID)
  std::unique_ptr<base::android::ApplicationStatusListener>
      app_status_listener_;
#endif

  // True if running on android and the app is in the background.
  bool app_on_background_ = false;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<LegacyCacheStorage> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(LegacyCacheStorage);
};

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_LEGACY_LEGACY_CACHE_STORAGE_H_

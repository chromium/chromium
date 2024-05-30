// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_H_
#define CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "components/services/storage/public/mojom/blob_storage_context.mojom.h"
#include "components/services/storage/public/mojom/cache_storage_control.mojom.h"
#include "content/browser/cache_storage/blob_storage_context_wrapper.h"
#include "content/browser/cache_storage/cache_storage_cache.h"
#include "content/browser/cache_storage/cache_storage_cache_observer.h"
#include "content/browser/cache_storage/cache_storage_handle.h"
#include "content/browser/cache_storage/cache_storage_manager.h"
#include "content/browser/cache_storage/cache_storage_scheduler_types.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "storage/browser/quota/quota_manager.h"
#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/application_status_listener.h"
#endif

namespace base {
class SequencedTaskRunner;
}

namespace content {
class CacheStorageIndex;
class CacheStorageScheduler;
class CacheStorageManager;

namespace cache_storage_manager_unittest {
class CacheStorageManagerTest;
FORWARD_DECLARE_TEST(CacheStorageManagerTest, PersistedCacheKeyUsed);
FORWARD_DECLARE_TEST(CacheStorageManagerTest, PutResponseWithExistingFileTest);
FORWARD_DECLARE_TEST(CacheStorageManagerTest, TestErrorInitializingCache);
}  // namespace cache_storage_manager_unittest

// TODO(jkarlin): Constrain the total bytes used per storage key.

// CacheStorage holds the set of caches for a given BucketLocator. It is
// owned by the CacheStorageManager. This class expects to be run
// on the IO thread. The asynchronous methods are executed serially.
class CONTENT_EXPORT CacheStorage : public CacheStorageCacheObserver {
 public:
  constexpr static int64_t kSizeUnknown = -1;

  using SizeCallback = base::OnceCallback<void(int64_t)>;

  using BoolAndErrorCallback =
      base::OnceCallback<void(bool, blink::mojom::CacheStorageError)>;
  using ErrorCallback =
      base::OnceCallback<void(blink::mojom::CacheStorageError)>;
  using CacheAndErrorCallback =
      base::OnceCallback<void(CacheStorageCacheHandle,
                              blink::mojom::CacheStorageError)>;
  using EnumerateCachesCallback =
      base::OnceCallback<void(std::vector<std::string> cache_names)>;

  static const char kIndexFileName[];

  CacheStorage(const base::FilePath& origin_path,
               bool memory_only,
               base::SequencedTaskRunner* cache_task_runner,
               scoped_refptr<base::SequencedTaskRunner> scheduler_task_runner,
               scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
               scoped_refptr<BlobStorageContextWrapper> blob_storage_context,
               CacheStorageManager* cache_storage_manager,
               const storage::BucketLocator& bucket_locator,
               storage::mojom::CacheStorageOwner owner);

  CacheStorage(const CacheStorage&) = delete;
  CacheStorage& operator=(const CacheStorage&) = delete;

  // Any unfinished asynchronous operations may not complete or call their
  // callbacks.
  virtual ~CacheStorage();

  // Creates a new handle to this CacheStorage instance. Each handle represents
  // a signal that the CacheStorage is in active use and should avoid cleaning
  // up resources, if possible. However, there are some cases, such as a
  // user-initiated storage wipe, that will forcibly delete the CacheStorage
  // instance. Therefore the handle should be treated as a weak pointer that
  // needs to be tested for existence before use.
  CacheStorageHandle CreateHandle();

  // These methods are called by the CacheStorageHandle to track the number
  // of outstanding references.
  void AddHandleRef();
  void DropHandleRef();
  void AssertUnreferenced() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(!handle_ref_count_);
  }

  // Explicitly begin initialization if it has not already been triggered.
  void Init();

  // Get the cache for the given key. If the cache is not found it is
  // created. The CacheStorgeCacheHandle in the callback prolongs the lifetime
  // of the cache. Once all handles to a cache are deleted the cache is deleted.
  // The cache will also be deleted in the CacheStorage's destructor so be sure
  // to check the handle's value before using it.
  void OpenCache(const std::string& cache_name,
                 int64_t trace_id,
                 CacheAndErrorCallback callback);

  // Calls the callback with whether or not the cache exists.
  void HasCache(const std::string& cache_name,
                int64_t trace_id,
                BoolAndErrorCallback callback);

  // Deletes the cache if it exists. If it doesn't exist,
  // blink::mojom::CacheStorageError::kErrorNotFound is returned. Any
  // existing CacheStorageCacheHandle(s) to the cache will remain valid but
  // future CacheStorage operations won't be able to access the cache. The cache
  // isn't actually erased from disk until the last handle is dropped.
  void DoomCache(const std::string& cache_name,
                 int64_t trace_id,
                 ErrorCallback callback);

  // Calls the callback with the existing cache names.
  void EnumerateCaches(int64_t trace_id, EnumerateCachesCallback callback);

  // Calls match on the cache with the given |cache_name|.
  void MatchCache(const std::string& cache_name,
                  blink::mojom::FetchAPIRequestPtr request,
                  blink::mojom::CacheQueryOptionsPtr match_options,
                  CacheStorageSchedulerPriority priority,
                  int64_t trace_id,
                  CacheStorageCache::ResponseCallback callback);

  // Calls match on all of the caches in parallel, calling |callback| with the
  // response from the first cache (in order of cache creation) to have the
  // entry. If no response is found then |callback| is called with
  // blink::mojom::CacheStorageError::kErrorNotFound.
  void MatchAllCaches(blink::mojom::FetchAPIRequestPtr request,
                      blink::mojom::CacheQueryOptionsPtr match_options,
                      CacheStorageSchedulerPriority priority,
                      int64_t trace_id,
                      CacheStorageCache::ResponseCallback callback);

  // Puts the request/response pair in the cache.
  void WriteToCache(const std::string& cache_name,
                    blink::mojom::FetchAPIRequestPtr request,
                    blink::mojom::FetchAPIResponsePtr response,
                    int64_t trace_id,
                    ErrorCallback callback);

  // Sums the sizes of each cache and closes them. Runs |callback| with the
  // size. The sizes include any doomed caches and will also force close all
  // caches even if there are existing handles to them.
  void GetSizeThenCloseAllCaches(SizeCallback callback);

  // The size of all of the storage key's contents. This value should be used as
  // an estimate only since the cache may be modified at any time.
  void Size(SizeCallback callback);

  // The functions below are for tests to verify that the operations run
  // serially.
  CacheStorageSchedulerId StartAsyncOperationForTesting();
  void CompleteAsyncOperationForTesting(CacheStorageSchedulerId id);

  // Removes the manager reference. Called before this storage is deleted by the
  // manager, since it is removed from manager's storage map before deleting.
  void ResetManager();

  // CacheStorageCacheObserver:
  void CacheSizeUpdated(const CacheStorageCache* cache) override;

  // Destroy any CacheStorageCache instances that are not currently referenced
  // by a CacheStorageCacheHandle.
  void ReleaseUnreferencedCaches();

  static CacheStorage* From(const CacheStorageHandle& handle) {
    return static_cast<CacheStorage*>(handle.value());
  }

 protected:
  // Virtual for testing
  virtual void CacheUnreferenced(CacheStorageCache* cache);

 private:
  friend class CacheStorageCache;
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

  typedef std::map<std::string, std::unique_ptr<CacheStorageCache>> CacheMap;

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
                                 std::unique_ptr<CacheStorageCache> cache,
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
  void DeleteCacheFinalize(CacheStorageCache* doomed_cache);
  void DeleteCacheDidGetSize(CacheStorageCache* doomed_cache,
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

#if BUILDFLAG(IS_ANDROID)
  void OnApplicationStateChange(base::android::ApplicationState state);
#endif

  // The `BucketLocator` that this CacheStorage is associated with.
  const storage::BucketLocator bucket_locator_;

  // Whether or not we've loaded the list of cache names into memory.
  bool initialized_ = false;
  bool initializing_ = false;

  // True if the backend is supposed to reside in memory only.
  const bool memory_only_;

  // The pending operation scheduler.
  std::unique_ptr<CacheStorageScheduler> scheduler_;

  // The map of cache names to CacheStorageCache objects.
  CacheMap cache_map_;

  // Caches that have been deleted but must still be held onto until all handles
  // have been released.
  std::map<CacheStorageCache*, std::unique_ptr<CacheStorageCache>>
      doomed_caches_;

  // The cache index data.
  std::unique_ptr<CacheStorageIndex> cache_index_;

  // The file path for this CacheStorage.
  base::FilePath directory_path_;

  // The TaskRunner to run file IO on.
  scoped_refptr<base::SequencedTaskRunner> cache_task_runner_;

  size_t handle_ref_count_ = 0;

  // Performs backend specific operations (memory vs disk).
  std::unique_ptr<CacheLoader> cache_loader_;

  // The quota manager.
  scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy_;

  // An IO thread bound wrapper for storage.mojom.BlobStorageContext.
  scoped_refptr<BlobStorageContextWrapper> blob_storage_context_;

  // The owner that this CacheStorage is associated with.
  const storage::mojom::CacheStorageOwner owner_;

  CacheStorageSchedulerId init_id_ = -1;

  // The manager that owns this cache storage. Only set to null by
  // RemoveManager() when this cache storage is being deleted.
  raw_ptr<CacheStorageManager> cache_storage_manager_;

  base::CancelableOnceClosure index_write_task_;

#if BUILDFLAG(IS_ANDROID)
  std::unique_ptr<base::android::ApplicationStatusListener>
      app_status_listener_;
#endif

  // True if running on android and the app is in the background.
  bool app_on_background_ = false;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<CacheStorage> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_H_

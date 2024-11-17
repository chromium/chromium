// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_HOST_GPU_DISK_CACHE_H_
#define GPU_IPC_HOST_GPU_DISK_CACHE_H_

#include <stdint.h>

#include <string>
#include <unordered_map>

#include "base/containers/flat_map.h"
#include "base/containers/queue.h"
#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "gpu/ipc/common/gpu_disk_cache_type.h"
#include "net/base/completion_once_callback.h"
#include "net/disk_cache/disk_cache.h"

namespace gpu {

class GpuDiskCacheFactory;
class GpuDiskCacheEntry;
class GpuDiskCacheReadHelper;
class GpuDiskCacheClearHelper;

// GpuDiskCache is the interface to the on disk cache for the GPU process.
class GpuDiskCache : public base::RefCounted<GpuDiskCache> {
 public:
  using BlobLoadedCallback =
      base::RepeatingCallback<void(const std::string&, const std::string&)>;

  GpuDiskCache(const GpuDiskCache&) = delete;
  GpuDiskCache& operator=(const GpuDiskCache&) = delete;

  // Store the |blob| into the cache under |key|.
  void Cache(const std::string& key, const std::string& blob);

  // Clear a range of entries. This supports unbounded deletes in either
  // direction by using null Time values for either |begin_time| or |end_time|.
  // The return value is a net error code. If this method returns
  // ERR_IO_PENDING, the |completion_callback| will be invoked when the
  // operation completes.
  int Clear(base::Time begin_time,
            base::Time end_time,
            net::CompletionOnceCallback completion_callback);

  // Sets a callback for when the cache is available. If the cache is
  // already available the callback will not be called and net::OK is returned.
  // If the callback is set net::ERR_IO_PENDING is returned and the callback
  // will be executed when the cache is available.
  int SetAvailableCallback(net::CompletionOnceCallback callback);

  // Returns the number of elements currently in the cache.
  int32_t Size();

  // Set a callback notification for when all current entries have been
  // written to the cache.
  // The return value is a net error code. If this method returns
  // ERR_IO_PENDING, the |callback| will be invoked when all entries have
  // been written to the cache.
  int SetCacheCompleteCallback(net::CompletionOnceCallback callback);

 private:
  friend class base::RefCounted<GpuDiskCache>;
  friend class GpuDiskCacheEntry;
  friend class GpuDiskCacheReadHelper;
  friend class GpuDiskCacheFactory;

  GpuDiskCache(GpuDiskCacheFactory* factory,
               const base::FilePath& cache_path,
               const BlobLoadedCallback& blob_loaded_cb,
               base::OnceClosure cache_destroyed_cb);
  ~GpuDiskCache();

  void Init();
  void CacheCreatedCallback(disk_cache::BackendResult rv);

  disk_cache::Backend* backend() { return backend_.get(); }

  void EntryComplete(GpuDiskCacheEntry* entry);
  void ReadComplete();

  raw_ptr<GpuDiskCacheFactory> factory_;
  bool cache_available_ = false;
  base::FilePath cache_path_;
  bool is_initialized_ = false;
  net::CompletionOnceCallback available_callback_;
  net::CompletionOnceCallback cache_complete_callback_;
  BlobLoadedCallback blob_loaded_cb_;
  base::OnceClosure cache_destroyed_cb_;

  std::unique_ptr<disk_cache::Backend> backend_;

  std::unique_ptr<GpuDiskCacheReadHelper> helper_;
  std::unordered_map<GpuDiskCacheEntry*, std::unique_ptr<GpuDiskCacheEntry>>
      entries_;
};

// GpuDiskCacheFactory maintains a cache of GpuDiskCache objects so we only
// create one per profile directory.
class GpuDiskCacheFactory {
 public:
  using HandleToPathMap = base::flat_map<GpuDiskCacheHandle, base::FilePath>;
  using BlobLoadedForCacheCallback = base::RepeatingCallback<
      void(const GpuDiskCacheHandle&, const std::string&, const std::string&)>;
  using CacheDestroyedCallback =
      base::OnceCallback<void(const GpuDiskCacheHandle&)>;

  // Constructor allows passing in reserved handles and their corresponding
  // paths.
  explicit GpuDiskCacheFactory(
      const HandleToPathMap& reserved_handles = HandleToPathMap());

  GpuDiskCacheFactory(const GpuDiskCacheFactory&) = delete;
  GpuDiskCacheFactory& operator=(const GpuDiskCacheFactory&) = delete;

  ~GpuDiskCacheFactory();

  // Clear the given gpu disk |cache|. This supports unbounded deletes in
  // either direction by using null Time values for either |begin_time| or
  // |end_time|. The |callback| will be executed when the clear is complete.
  void ClearByCache(scoped_refptr<GpuDiskCache> cache,
                    base::Time begin_time,
                    base::Time end_time,
                    base::OnceClosure callback);

  // Clear the gpu disk cache for the given |path|. This supports unbounded
  // deletes in either direction by using null Time values for either
  // |begin_time| or |end_time|. The |callback| will be executed when the
  // clear is complete.
  void ClearByPath(const base::FilePath& path,
                   base::Time begin_time,
                   base::Time end_time,
                   base::OnceClosure callback);

  // Looks up a |path| and returns a cache handle for it (registering it if
  // necessary) for the given |type|.
  GpuDiskCacheHandle GetCacheHandle(GpuDiskCacheType type,
                                    const base::FilePath& path);

  // Releases the cache's handle (or at least one count of it). When a cache
  // handle's number of refs goes to 0, we can cull it from from the map. If the
  // handle is a reserved handle, nothing happens because we do not cull
  // reserved handles.
  void ReleaseCacheHandle(GpuDiskCache* cache);

  // Retrieve the gpu disk cache with the given |handle| if the handle has an
  // associated path. Returns nullptr if there is no associated path or the
  // cache was never explicitly created.
  scoped_refptr<GpuDiskCache> Get(const GpuDiskCacheHandle& handle);

  // Creates an non-existing gpu cache given a handle that was previously
  // registered via GetCacheHandle. Returns nullptr if there was no associated
  // path.
  scoped_refptr<GpuDiskCache> Create(
      const GpuDiskCacheHandle& handle,
      const BlobLoadedForCacheCallback& blob_loaded_cb = base::DoNothing(),
      CacheDestroyedCallback cache_destroyed_cb = base::DoNothing());

  // Set the provided |cache| into the cache map for the given |path|.
  void AddToCache(const base::FilePath& path, GpuDiskCache* cache);

  // Remove the provided |path| from our cache map.
  void RemoveFromCache(const base::FilePath& path);

 private:
  friend class GpuDiskCacheClearHelper;

  scoped_refptr<GpuDiskCache> GetOrCreateByPath(
      const base::FilePath& path,
      const GpuDiskCache::BlobLoadedCallback& blob_loaded_cb =
          base::DoNothing(),
      base::OnceClosure cache_destroyed_cb = base::DoNothing());

  void CacheCleared(GpuDiskCache* cache);

  THREAD_CHECKER(thread_checker_);

  // Implementation of bi-directional mapping from path to handle and handle to
  // path for both way lookup. Entries in these maps are removed when the last
  // scoped_ptr of the respective cache is destroyed, unless the handle is a
  // special reserved handle, in which case we do not remove the entry.
  HandleToPathMap handle_to_path_map_;
  using PathToHandleMap =
      base::flat_map<base::FilePath, gpu::GpuDiskCacheHandle>;
  PathToHandleMap path_to_handle_map_;

  // Map that essentially ref-counts the number of times that a handle is
  // re-used. This is important since it allows us to cull the bi-directional
  // mappings above when we no longer need them. Handle ref-count is incremented
  // each time it is returned in GetCacheHandle, and decremented/removed in
  // ReleaseCacheHandle. Note that this does not apply for reserved handles
  // which we never cull.
  using HandleRefCounts = base::flat_map<GpuDiskCacheHandle, uint32_t>;
  HandleRefCounts handle_ref_counts_;

  using PathToCacheMap =
      base::flat_map<base::FilePath, raw_ptr<GpuDiskCache, CtnExperimental>>;
  PathToCacheMap gpu_cache_map_;

  using ClearHelperQueue =
      base::queue<std::unique_ptr<GpuDiskCacheClearHelper>>;
  using CacheToClearHelperQueueMap =
      base::flat_map<GpuDiskCache*, ClearHelperQueue>;
  CacheToClearHelperQueueMap gpu_clear_map_;

  // Handles are all int32_t types underneath so we can allocate them all via
  // this incrementing internal counter.
  int32_t next_available_handle_ = 0;
};

}  // namespace gpu

#endif  // GPU_IPC_HOST_GPU_DISK_CACHE_H_

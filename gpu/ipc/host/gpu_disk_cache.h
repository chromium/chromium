// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_HOST_GPU_DISK_CACHE_H_
#define GPU_IPC_HOST_GPU_DISK_CACHE_H_

#include <stdint.h>

#include <map>
#include <string>
#include <unordered_map>

#include "base/containers/queue.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
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

  void SetBlobLoadedCallback(const BlobLoadedCallback& callback) {
    blob_loaded_callback_ = callback;
  }

  // Store the |blob| into the cache under |key|.
  void Cache(const std::string& key, const std::string& blob);

  // Clear a range of entries. This supports unbounded deletes in either
  // direction by using null Time values for either |begin_time| or |end_time|.
  // The return value is a net error code. If this method returns
  // ERR_IO_PENDING, the |completion_callback| will be invoked when the
  // operation completes.
  int Clear(const base::Time begin_time,
            const base::Time end_time,
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

  // Returns the size which should be used for the gpu disk cache.
  static size_t CacheSizeBytes();

 private:
  friend class base::RefCounted<GpuDiskCache>;
  friend class GpuDiskCacheEntry;
  friend class GpuDiskCacheReadHelper;
  friend class GpuDiskCacheFactory;

  GpuDiskCache(GpuDiskCacheFactory* factory, const base::FilePath& cache_path);
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
  BlobLoadedCallback blob_loaded_callback_;

  std::unique_ptr<disk_cache::Backend> backend_;

  std::unique_ptr<GpuDiskCacheReadHelper> helper_;
  std::unordered_map<GpuDiskCacheEntry*, std::unique_ptr<GpuDiskCacheEntry>>
      entries_;
};

// GpuDiskCacheFactory maintains a cache of GpuDiskCache objects so we only
// create one per profile directory.
class GpuDiskCacheFactory {
 public:
  GpuDiskCacheFactory();

  GpuDiskCacheFactory(const GpuDiskCacheFactory&) = delete;
  GpuDiskCacheFactory& operator=(const GpuDiskCacheFactory&) = delete;

  ~GpuDiskCacheFactory();

  // Clear the gpu disk cache for the given |path|. This supports unbounded
  // deletes in either direction by using null Time values for either
  // |begin_time| or |end_time|. The |callback| will be executed when the
  // clear is complete.
  void ClearByPath(const base::FilePath& path,
                   const base::Time& begin_time,
                   const base::Time& end_time,
                   base::OnceClosure callback);

  // Same as ClearByPath, but looks up the cache by |client_id|. The |callback|
  // will be executed when the clear is complete.
  void ClearByClientId(int32_t client_id,
                       const base::Time& begin_time,
                       const base::Time& end_time,
                       base::OnceClosure callback);

  // Retrieve the gpu disk cache for the provided |client_id|.
  scoped_refptr<GpuDiskCache> Get(int32_t client_id);

  // Set the |path| to be used for the disk cache for |client_id|.
  void SetCacheInfo(int32_t client_id, const base::FilePath& path);

  // Remove the path mapping for |client_id|.
  void RemoveCacheInfo(int32_t client_id);

  // Set the provided |cache| into the cache map for the given |path|.
  void AddToCache(const base::FilePath& path, GpuDiskCache* cache);

  // Remove the provided |path| from our cache map.
  void RemoveFromCache(const base::FilePath& path);

 private:
  friend class GpuDiskCacheClearHelper;

  scoped_refptr<GpuDiskCache> GetByPath(const base::FilePath& path);
  void CacheCleared(const base::FilePath& path);

  THREAD_CHECKER(thread_checker_);

  using PathToCacheMap = std::map<base::FilePath, GpuDiskCache*>;
  PathToCacheMap gpu_cache_map_;

  using ClientIdToPathMap = std::map<int32_t, base::FilePath>;
  ClientIdToPathMap client_id_to_path_map_;

  using ClearHelperQueue =
      base::queue<std::unique_ptr<GpuDiskCacheClearHelper>>;
  using PathToClearHelperQueueMap = std::map<base::FilePath, ClearHelperQueue>;
  PathToClearHelperQueueMap gpu_clear_map_;
};

}  // namespace gpu

#endif  // GPU_IPC_HOST_GPU_DISK_CACHE_H_

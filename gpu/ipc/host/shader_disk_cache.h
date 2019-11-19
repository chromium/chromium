// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_HOST_SHADER_DISK_CACHE_H_
#define GPU_IPC_HOST_SHADER_DISK_CACHE_H_

#include <stdint.h>

#include <map>
#include <string>
#include <unordered_map>

#include "base/containers/queue.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "net/base/completion_once_callback.h"
#include "net/disk_cache/disk_cache.h"

namespace gpu {

class ShaderCacheFactory;
class ShaderDiskCacheEntry;
class ShaderDiskReadHelper;
class ShaderClearHelper;

// ShaderDiskCache is the interface to the on disk cache for
// GL shaders.
class ShaderDiskCache : public base::RefCounted<ShaderDiskCache> {
 public:
  using ShaderLoadedCallback =
      base::RepeatingCallback<void(const std::string&, const std::string&)>;

  void set_shader_loaded_callback(const ShaderLoadedCallback& callback) {
    shader_loaded_callback_ = callback;
  }

  // Store the |shader| into the cache under |key|.
  void Cache(const std::string& key, const std::string& shader);

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

  // Returns the size which should be used for the shader disk cache.
  static size_t CacheSizeBytes();

 private:
  friend class base::RefCounted<ShaderDiskCache>;
  friend class ShaderDiskCacheEntry;
  friend class ShaderDiskReadHelper;
  friend class ShaderCacheFactory;

  ShaderDiskCache(ShaderCacheFactory* factory,
                  const base::FilePath& cache_path);
  ~ShaderDiskCache();

  void Init();
  void CacheCreatedCallback(int rv);

  disk_cache::Backend* backend() { return backend_.get(); }

  void EntryComplete(ShaderDiskCacheEntry* entry);
  void ReadComplete();

  ShaderCacheFactory* factory_;
  bool cache_available_;
  base::FilePath cache_path_;
  bool is_initialized_;
  net::CompletionOnceCallback available_callback_;
  net::CompletionOnceCallback cache_complete_callback_;
  ShaderLoadedCallback shader_loaded_callback_;

  std::unique_ptr<disk_cache::Backend> backend_;

  std::unique_ptr<ShaderDiskReadHelper> helper_;
  std::unordered_map<ShaderDiskCacheEntry*,
                     std::unique_ptr<ShaderDiskCacheEntry>>
      entries_;

  DISALLOW_COPY_AND_ASSIGN(ShaderDiskCache);
};

// ShaderCacheFactory maintains a cache of ShaderDiskCache objects
// so we only create one per profile directory.
class ShaderCacheFactory : public base::ThreadChecker {
 public:
  ShaderCacheFactory();
  ~ShaderCacheFactory();

  // Clear the shader disk cache for the given |path|. This supports unbounded
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

  // Retrieve the shader disk cache for the provided |client_id|.
  scoped_refptr<ShaderDiskCache> Get(int32_t client_id);

  // Set the |path| to be used for the disk cache for |client_id|.
  void SetCacheInfo(int32_t client_id, const base::FilePath& path);

  // Remove the path mapping for |client_id|.
  void RemoveCacheInfo(int32_t client_id);

  // Set the provided |cache| into the cache map for the given |path|.
  void AddToCache(const base::FilePath& path, ShaderDiskCache* cache);

  // Remove the provided |path| from our cache map.
  void RemoveFromCache(const base::FilePath& path);

 private:
  friend class ShaderClearHelper;

  scoped_refptr<ShaderDiskCache> GetByPath(const base::FilePath& path);
  void CacheCleared(const base::FilePath& path);

  using ShaderCacheMap = std::map<base::FilePath, ShaderDiskCache*>;
  ShaderCacheMap shader_cache_map_;

  using ClientIdToPathMap = std::map<int32_t, base::FilePath>;
  ClientIdToPathMap client_id_to_path_map_;

  using ShaderClearQueue = base::queue<std::unique_ptr<ShaderClearHelper>>;
  using ShaderClearMap = std::map<base::FilePath, ShaderClearQueue>;
  ShaderClearMap shader_clear_map_;

  DISALLOW_COPY_AND_ASSIGN(ShaderCacheFactory);
};

}  // namespace gpu

#endif  // GPU_IPC_HOST_SHADER_DISK_CACHE_H_

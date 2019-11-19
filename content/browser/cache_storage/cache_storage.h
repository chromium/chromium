// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_H_
#define CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "content/browser/cache_storage/cache_storage_cache.h"
#include "content/browser/cache_storage/cache_storage_handle.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom.h"
#include "url/origin.h"

namespace content {

// CacheStorage holds the set of caches for a given origin. It is
// owned by the CacheStorageManager. This class expects to be run
// on the IO thread. The asynchronous methods are executed serially.
class CONTENT_EXPORT CacheStorage {
 public:
  constexpr static int64_t kSizeUnknown = -1;

  using BoolAndErrorCallback =
      base::OnceCallback<void(bool, blink::mojom::CacheStorageError)>;
  using ErrorCallback =
      base::OnceCallback<void(blink::mojom::CacheStorageError)>;
  using CacheAndErrorCallback =
      base::OnceCallback<void(CacheStorageCacheHandle,
                              blink::mojom::CacheStorageError)>;
  using EnumerateCachesCallback =
      base::OnceCallback<void(std::vector<std::string> cache_names)>;

  // Creates a new handle to this CacheStorage instance. Each handle represents
  // a signal that the CacheStorage is in active use and should avoid cleaning
  // up resources, if possible. However, there are some cases, such as a
  // user-initiated storage wipe, that will forcibly delete the CacheStorage
  // instance. Therefore the handle should be treated as a weak pointer that
  // needs to be tested for existence before use.
  virtual CacheStorageHandle CreateHandle() = 0;
  virtual void AddHandleRef() = 0;
  virtual void DropHandleRef() = 0;

  // Get the cache for the given key. If the cache is not found it is
  // created. The CacheStorgeCacheHandle in the callback prolongs the lifetime
  // of the cache. Once all handles to a cache are deleted the cache is deleted.
  // The cache will also be deleted in the CacheStorage's destructor so be sure
  // to check the handle's value before using it.
  virtual void OpenCache(const std::string& cache_name,
                         int64_t trace_id,
                         CacheAndErrorCallback callback) = 0;

  // Calls the callback with whether or not the cache exists.
  virtual void HasCache(const std::string& cache_name,
                        int64_t trace_id,
                        BoolAndErrorCallback callback) = 0;

  // Deletes the cache if it exists. If it doesn't exist,
  // blink::mojom::CacheStorageError::kErrorNotFound is returned. Any
  // existing CacheStorageCacheHandle(s) to the cache will remain valid but
  // future CacheStorage operations won't be able to access the cache. The cache
  // isn't actually erased from disk until the last handle is dropped.
  virtual void DoomCache(const std::string& cache_name,
                         int64_t trace_id,
                         ErrorCallback callback) = 0;

  // Calls the callback with the existing cache names.
  virtual void EnumerateCaches(int64_t trace_id,
                               EnumerateCachesCallback callback) = 0;

  // Calls match on the cache with the given |cache_name|.
  virtual void MatchCache(const std::string& cache_name,
                          blink::mojom::FetchAPIRequestPtr request,
                          blink::mojom::CacheQueryOptionsPtr match_options,
                          CacheStorageSchedulerPriority priority,
                          int64_t trace_id,
                          CacheStorageCache::ResponseCallback callback) = 0;

  // Calls match on all of the caches in parallel, calling |callback| with the
  // response from the first cache (in order of cache creation) to have the
  // entry. If no response is found then |callback| is called with
  // blink::mojom::CacheStorageError::kErrorNotFound.
  virtual void MatchAllCaches(blink::mojom::FetchAPIRequestPtr request,
                              blink::mojom::CacheQueryOptionsPtr match_options,
                              CacheStorageSchedulerPriority priority,
                              int64_t trace_id,
                              CacheStorageCache::ResponseCallback callback) = 0;

  // Puts the request/response pair in the cache.
  virtual void WriteToCache(const std::string& cache_name,
                            blink::mojom::FetchAPIRequestPtr request,
                            blink::mojom::FetchAPIResponsePtr response,
                            int64_t trace_id,
                            ErrorCallback callback) = 0;

  // The immutable origin of the CacheStorage.
  const url::Origin& Origin() const { return origin_; }

 protected:
  explicit CacheStorage(const url::Origin& origin);
  virtual ~CacheStorage() = default;

  // The origin that this CacheStorage is associated with.
  const url::Origin origin_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_H_

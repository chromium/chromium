// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_CACHE_OBSERVER_H_
#define CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_CACHE_OBSERVER_H_

namespace content {

class CacheStorageCache;

class CacheStorageCacheObserver {
 public:
  // The cache size has been set.
  virtual void CacheSizeUpdated(const CacheStorageCache* cache) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_CACHE_OBSERVER_H_

// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_INDEX_H_
#define CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_INDEX_H_

#include <list>
#include <string>
#include <unordered_map>

#include "base/gtest_prod_util.h"
#include "content/browser/cache_storage/cache_storage.h"
#include "content/common/content_export.h"

namespace content {

// CacheStorageIndex maintains an ordered list of metadata (CacheMetadata)
// for each cache owned by a CacheStorage object. This class is not thread safe,
// and is owned by the CacheStorage.
class CONTENT_EXPORT CacheStorageIndex {
 public:
  struct CacheMetadata {
    CacheMetadata(const std::string& name, int64_t size, int64_t padding)
        : name(name), size(size), padding(padding) {}
    std::string name;
    // The size (in bytes) of the cache. Set to CacheStorage::kSizeUnknown if
    // size not known.
    int64_t size;

    // The padding (in bytes) of the cache. Set to CacheStorage::kSizeUnknown
    // if padding not known.
    int64_t padding;

    // The algorithm version used to calculate this padding.
    int32_t padding_version;
  };

  CacheStorageIndex();

  CacheStorageIndex(const CacheStorageIndex&) = delete;
  CacheStorageIndex& operator=(const CacheStorageIndex&) = delete;

  ~CacheStorageIndex();

  CacheStorageIndex& operator=(CacheStorageIndex&& rhs);

  void Insert(const CacheMetadata& cache_metadata);
  void Delete(const std::string& cache_name);

  // Sets the actual (unpadded) cache size. Returns true if the new size is
  // different than the current size else false.
  bool SetCacheSize(const std::string& cache_name, int64_t size);

  // Get the cache metadata for a given cache name. If not found nullptr is
  // returned.
  const CacheMetadata* GetMetadata(const std::string& cache_name) const;

  // Sets the cache padding. Returns true if the new padding is different than
  // the current padding else false.
  bool SetCachePadding(const std::string& cache_name, int64_t padding);

  const std::list<CacheMetadata>& ordered_cache_metadata() const {
    return ordered_cache_metadata_;
  }

  size_t num_entries() const { return ordered_cache_metadata_.size(); }

  // Will calculate (if necessary), and return the total sum of all cache sizes.
  int64_t GetPaddedStorageSize();

  // Mark the cache as doomed. This removes the cache metadata from the index.
  // All const methods (eg: num_entries) will behave as if the doomed cache is
  // not present in the index. Prior to calling any non-const method the doomed
  // cache must either be finalized (by calling FinalizeDoomedCache) or restored
  // (by calling RestoreDoomedCache).
  //
  // RestoreDoomedCache restores the metadata to the index at the original
  // position prior to calling DoomCache.
  void DoomCache(const std::string& cache_name);
  void FinalizeDoomedCache();
  void RestoreDoomedCache();

 private:
  FRIEND_TEST_ALL_PREFIXES(CacheStorageIndexTest, TestSetCacheSize);
  FRIEND_TEST_ALL_PREFIXES(CacheStorageIndexTest, TestSetCachePadding);

  void UpdateStorageSize();
  void CalculateStoragePadding();
  void ClearDoomedCache();

  // Return the size (in bytes) of the specified cache. Will return
  // CacheStorage::kSizeUnknown if the specified cache does not exist.
  int64_t GetCacheSizeForTesting(const std::string& cache_name) const;

  // Return the padding (in bytes) of the specified cache. Will return
  // CacheStorage::kSizeUnknown if the specified cache does not exist.
  int64_t GetCachePaddingForTesting(const std::string& cache_name) const;

  // Use a list to keep saved iterators valid during insert/erase.
  // Note: ordered by cache creation.
  std::list<CacheMetadata> ordered_cache_metadata_;
  std::unordered_map<std::string, std::list<CacheMetadata>::iterator>
      cache_metadata_map_;

  // The total unpadded size of all caches in this store.
  int64_t storage_size_ = CacheStorage::kSizeUnknown;

  // The total padding of all caches in this store.
  int64_t storage_padding_ = CacheStorage::kSizeUnknown;

  // The doomed cache metadata saved when calling DoomCache.
  CacheMetadata doomed_cache_metadata_;
  std::list<CacheMetadata>::iterator after_doomed_cache_metadata_;
  bool has_doomed_cache_ = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_INDEX_H_

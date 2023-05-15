// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_BUILT_IN_SHADER_CACHE_LOADER_H_
#define GPU_IPC_SERVICE_BUILT_IN_SHADER_CACHE_LOADER_H_

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/synchronization/waitable_event.h"
#include "gpu/ipc/service/gpu_ipc_service_export.h"

namespace gpu {

// BuiltInShaderCacheLoader loads the metal shaders that are packaged with
// chrome. Loading happens in the background. StartLoading() should be called
// early in startup to start the loading, and later on the values taken by way
// of TakeEntries().
class GPU_IPC_SERVICE_EXPORT BuiltInShaderCacheLoader {
 public:
  struct CacheEntry {
    CacheEntry();
    CacheEntry(CacheEntry&& other);
    ~CacheEntry();

    std::vector<uint8_t> key;
    std::vector<uint8_t> value;
  };

  // Starts loading the cache.
  static void StartLoading();

  // Returns the entries from the cache and deletes it. This blocks until the
  // cache is loaded.
  static std::unique_ptr<std::vector<CacheEntry>> TakeEntries();

 private:
  FRIEND_TEST_ALL_PREFIXES(BuiltInShaderCacheTest, Basic);

  BuiltInShaderCacheLoader();
  ~BuiltInShaderCacheLoader();

  // Called on the background thread to load the cache.
  void Load(const base::FilePath& path = base::FilePath());

  // Does the real work of loading the cache.
  void LoadImpl(const base::FilePath& path);

  // Returns the entries from the cache and deletes it. This blocks until the
  // cache is loaded.
  std::unique_ptr<std::vector<CacheEntry>> TakeEntriesImpl();

  std::vector<BuiltInShaderCacheLoader::CacheEntry> entries_;

  // Signaled when the cache is finished loading.
  base::WaitableEvent loaded_signaler_;
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_BUILT_IN_SHADER_CACHE_LOADER_H_

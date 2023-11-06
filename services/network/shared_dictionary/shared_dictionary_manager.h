// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_MANAGER_H_
#define SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_MANAGER_H_

#include <map>

#include "base/component_export.h"
#include "base/containers/lru_cache.h"
#include "base/functional/callback.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "net/disk_cache/disk_cache.h"
#include "net/extras/shared_dictionary/shared_dictionary_isolation_key.h"
#include "net/extras/shared_dictionary/shared_dictionary_usage_info.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace base {
class FilePath;
}  //  namespace base

namespace disk_cache {
class BackendFileOperationsFactory;
}  // namespace disk_cache

namespace network {
namespace cors {
class CorsURLLoaderSharedDictionaryTest;
}

class SharedDictionaryStorage;

// This class is attached to NetworkContext and manages the dictionaries for
// CompressionDictionaryTransport feature.
class COMPONENT_EXPORT(NETWORK_SERVICE) SharedDictionaryManager {
 public:
  // Returns a SharedDictionaryManager which keeps the whole dictionary
  // information in memory.
  static std::unique_ptr<SharedDictionaryManager> CreateInMemory(
      uint64_t cache_max_size,
      uint64_t cache_max_count);

  // Returns a SharedDictionaryManager which keeps the dictionary information
  // on disk.
  static std::unique_ptr<SharedDictionaryManager> CreateOnDisk(
      const base::FilePath& database_path,
      const base::FilePath& cache_directory_path,
      uint64_t cache_max_size,
      uint64_t cache_max_count,
#if BUILDFLAG(IS_ANDROID)
      disk_cache::ApplicationStatusListenerGetter app_status_listener_getter,
#endif  // BUILDFLAG(IS_ANDROID)
      scoped_refptr<disk_cache::BackendFileOperationsFactory>
          file_operations_factory);

  SharedDictionaryManager(const SharedDictionaryManager&) = delete;
  SharedDictionaryManager& operator=(const SharedDictionaryManager&) = delete;

  virtual ~SharedDictionaryManager();

  // Returns a SharedDictionaryStorage for the `isolation_key`.
  scoped_refptr<SharedDictionaryStorage> GetStorage(
      const net::SharedDictionaryIsolationKey& isolation_key);

  // Called when the SharedDictionaryStorage for the `isolation_key` is
  // deleted.
  void OnStorageDeleted(const net::SharedDictionaryIsolationKey& isolation_key);

  // Sets the max size of shared dictionary cache.
  virtual void SetCacheMaxSize(uint64_t cache_max_size) = 0;
  virtual void ClearData(base::Time start_time,
                         base::Time end_time,
                         base::RepeatingCallback<bool(const GURL&)> url_matcher,
                         base::OnceClosure callback) = 0;
  virtual void ClearDataForIsolationKey(
      const net::SharedDictionaryIsolationKey& isolation_key,
      base::OnceClosure callback) = 0;
  virtual void GetUsageInfo(
      base::OnceCallback<void(
          const std::vector<net::SharedDictionaryUsageInfo>&)> callback) = 0;
  virtual void GetSharedDictionaryInfo(
      const net::SharedDictionaryIsolationKey& isolation_key,
      base::OnceCallback<void(
          std::vector<network::mojom::SharedDictionaryInfoPtr>)> callback) = 0;
  virtual void GetOriginsBetween(
      base::Time start_time,
      base::Time end_time,
      base::OnceCallback<void(const std::vector<url::Origin>&)> callback) = 0;

 protected:
  SharedDictionaryManager();

  // Called to create a SharedDictionaryStorage for the `isolation_key`. This is
  // called only when there is no matching storage in `storages_`.
  virtual scoped_refptr<SharedDictionaryStorage> CreateStorage(
      const net::SharedDictionaryIsolationKey& isolation_key) = 0;

  base::WeakPtr<SharedDictionaryManager> GetWeakPtr();

  std::map<net::SharedDictionaryIsolationKey, raw_ptr<SharedDictionaryStorage>>&
  storages() {
    return storages_;
  }

 private:
  friend class cors::CorsURLLoaderSharedDictionaryTest;

  size_t GetStorageCountForTesting();

  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel level);

  base::LRUCache<net::SharedDictionaryIsolationKey,
                 scoped_refptr<SharedDictionaryStorage>>
      cached_storages_;
  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;
  base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level_ =
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;

  std::map<net::SharedDictionaryIsolationKey, raw_ptr<SharedDictionaryStorage>>
      storages_;

  base::WeakPtrFactory<SharedDictionaryManager> weak_factory_{this};
};

// Creates a network::mojom::SharedDictionaryInfo from a `DictionaryInfoType`.
// This is a template method because SharedDictionaryManagerOnDisk and
// SharedDictionaryManagerInMemory are using different class for
// DictionaryInfoType.
template <class DictionaryInfoType>
network::mojom::SharedDictionaryInfoPtr ToMojoSharedDictionaryInfo(
    const DictionaryInfoType& info) {
  auto mojo_info = network::mojom::SharedDictionaryInfo::New();
  mojo_info->match = info.match();
  mojo_info->dictionary_url = info.url();
  mojo_info->response_time = info.response_time();
  mojo_info->expiration = info.expiration();
  mojo_info->last_used_time = info.last_used_time();
  mojo_info->size = info.size();
  mojo_info->hash = info.hash();
  return mojo_info;
}

}  // namespace network

#endif  // SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_MANAGER_H_

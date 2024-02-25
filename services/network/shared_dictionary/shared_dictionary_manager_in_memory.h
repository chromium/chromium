// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_MANAGER_IN_MEMORY_H_
#define SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_MANAGER_IN_MEMORY_H_

#include "base/memory/weak_ptr.h"
#include "services/network/shared_dictionary/shared_dictionary_manager.h"

namespace net {
class SchemefulSite;
}  // namespace net

namespace network {

class SharedDictionaryStorage;

// A SharedDictionaryManager which keeps all dictionary information in memory.
class SharedDictionaryManagerInMemory : public SharedDictionaryManager {
 public:
  explicit SharedDictionaryManagerInMemory(uint64_t cache_max_size,
                                           uint64_t cache_max_count);
  ~SharedDictionaryManagerInMemory() override;

  SharedDictionaryManagerInMemory(const SharedDictionaryManagerInMemory&) =
      delete;
  SharedDictionaryManagerInMemory& operator=(
      const SharedDictionaryManagerInMemory&) = delete;

  // SharedDictionaryManager
  scoped_refptr<SharedDictionaryStorage> CreateStorage(
      const net::SharedDictionaryIsolationKey& isolation_key) override;
  void SetCacheMaxSize(uint64_t cache_max_size) override;
  void ClearData(base::Time start_time,
                 base::Time end_time,
                 base::RepeatingCallback<bool(const GURL&)> url_matcher,
                 base::OnceClosure callback) override;
  void ClearDataForIsolationKey(
      const net::SharedDictionaryIsolationKey& isolation_key,
      base::OnceClosure callback) override;
  void GetUsageInfo(base::OnceCallback<
                    void(const std::vector<net::SharedDictionaryUsageInfo>&)>
                        callback) override;
  void GetSharedDictionaryInfo(
      const net::SharedDictionaryIsolationKey& isolation_key,
      base::OnceCallback<
          void(std::vector<network::mojom::SharedDictionaryInfoPtr>)> callback)
      override;
  void GetOriginsBetween(
      base::Time start_time,
      base::Time end_time,
      base::OnceCallback<void(const std::vector<url::Origin>&)> callback)
      override;

  void MaybeRunCacheEvictionPerSite(const net::SchemefulSite& top_frame_site);
  void MaybeRunCacheEviction();

 private:
  // Performs a cache eviction using the specified params. If `top_frame_site`
  // is nullopt, performs the cache eviction for the all storages. Otherwise,
  // performs the cache eviction for the specified `top_frame_site`'s storages.
  void RunCacheEvictionImpl(std::optional<net::SchemefulSite> top_frame_site,
                            uint64_t max_size,
                            uint64_t size_low_watermark,
                            uint64_t max_count,
                            uint64_t count_low_watermark);

  uint64_t cache_max_size_;
  const uint64_t cache_max_count_;
  base::WeakPtrFactory<SharedDictionaryManagerInMemory> weak_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_MANAGER_IN_MEMORY_H_

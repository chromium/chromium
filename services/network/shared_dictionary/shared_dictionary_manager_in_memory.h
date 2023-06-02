// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_MANAGER_IN_MEMORY_H_
#define SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_MANAGER_IN_MEMORY_H_

#include "services/network/shared_dictionary/shared_dictionary_manager.h"

namespace network {

class SharedDictionaryStorage;

// A SharedDictionaryManager which keeps all dictionary information in memory.
class SharedDictionaryManagerInMemory : public SharedDictionaryManager {
 public:
  explicit SharedDictionaryManagerInMemory(uint64_t cache_max_size);

  SharedDictionaryManagerInMemory(const SharedDictionaryManagerInMemory&) =
      delete;
  SharedDictionaryManagerInMemory& operator=(
      const SharedDictionaryManagerInMemory&) = delete;

  // SharedDictionaryManager
  scoped_refptr<SharedDictionaryStorage> CreateStorage(
      const net::SharedDictionaryStorageIsolationKey& isolation_key) override;
  void SetCacheMaxSize(uint64_t cache_max_size) override;
  void ClearData(base::Time start_time,
                 base::Time end_time,
                 base::RepeatingCallback<bool(const GURL&)> url_matcher,
                 base::OnceClosure callback) override;

 private:
  uint64_t cache_max_size_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_MANAGER_IN_MEMORY_H_

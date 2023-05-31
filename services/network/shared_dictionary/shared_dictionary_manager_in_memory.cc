// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/shared_dictionary/shared_dictionary_manager_in_memory.h"

#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "services/network/shared_dictionary/shared_dictionary_storage_in_memory.h"

namespace network {

SharedDictionaryManagerInMemory::SharedDictionaryManagerInMemory(
    uint64_t cache_max_size)
    : cache_max_size_(cache_max_size) {}

scoped_refptr<SharedDictionaryStorage>
SharedDictionaryManagerInMemory::CreateStorage(
    const net::SharedDictionaryStorageIsolationKey& isolation_key) {
  return base::MakeRefCounted<SharedDictionaryStorageInMemory>(
      base::ScopedClosureRunner(
          base::BindOnce(&SharedDictionaryManager::OnStorageDeleted,
                         GetWeakPtr(), isolation_key)));
}

void SharedDictionaryManagerInMemory::SetCacheMaxSize(uint64_t cache_max_size) {
  // TODO(crbug.com/1413922): Implement cache eviction logic using
  // `cache_max_size_`.
  cache_max_size_ = cache_max_size;
}

void SharedDictionaryManagerInMemory::ClearData(
    base::Time start_time,
    base::Time end_time,
    base::RepeatingCallback<bool(const GURL&)> url_matcher,
    base::OnceClosure callback) {
  // TODO(crbug.com/1413922): Implement this.
  NOTIMPLEMENTED();
  std::move(callback).Run();
}

}  // namespace network

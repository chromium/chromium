// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/shared_dictionary/shared_dictionary_manager.h"

#include "services/network/shared_dictionary/shared_dictionary_manager_in_memory.h"
#include "services/network/shared_dictionary/shared_dictionary_manager_on_disk.h"
#include "services/network/shared_dictionary/shared_dictionary_storage.h"

namespace network {

// static
std::unique_ptr<SharedDictionaryManager>
SharedDictionaryManager::CreateInMemory(uint64_t cache_max_size,
                                        uint64_t cache_max_count) {
  return std::make_unique<SharedDictionaryManagerInMemory>(cache_max_size,
                                                           cache_max_count);
}

// static
std::unique_ptr<SharedDictionaryManager> SharedDictionaryManager::CreateOnDisk(
    const base::FilePath& database_path,
    const base::FilePath& cache_directory_path,
    uint64_t cache_max_size,
    uint64_t cache_max_count,
#if BUILDFLAG(IS_ANDROID)
    base::android::ApplicationStatusListener* app_status_listener,
#endif  // BUILDFLAG(IS_ANDROID)
    scoped_refptr<disk_cache::BackendFileOperationsFactory>
        file_operations_factory) {
  return std::make_unique<SharedDictionaryManagerOnDisk>(
      database_path, cache_directory_path, cache_max_size, cache_max_count,
#if BUILDFLAG(IS_ANDROID)
      app_status_listener,
#endif  // BUILDFLAG(IS_ANDROID)
      std::move(file_operations_factory));
}

SharedDictionaryManager::SharedDictionaryManager() = default;
SharedDictionaryManager::~SharedDictionaryManager() = default;

scoped_refptr<SharedDictionaryStorage> SharedDictionaryManager::GetStorage(
    const net::SharedDictionaryIsolationKey& isolation_key) {
  auto it = storages_.find(isolation_key);
  if (it != storages_.end()) {
    DCHECK(it->second);
    return it->second.get();
  }
  scoped_refptr<SharedDictionaryStorage> storage = CreateStorage(isolation_key);
  CHECK(storage);
  storages_.emplace(isolation_key, storage.get());
  return storage;
}

void SharedDictionaryManager::OnStorageDeleted(
    const net::SharedDictionaryIsolationKey& isolation_key) {
  size_t removed_count = storages_.erase(isolation_key);
  DCHECK_EQ(1U, removed_count);
}

base::WeakPtr<SharedDictionaryManager> SharedDictionaryManager::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace network

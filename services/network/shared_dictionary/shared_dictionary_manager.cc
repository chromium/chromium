// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/shared_dictionary/shared_dictionary_manager.h"

#include "base/location.h"
#include "services/network/shared_dictionary/shared_dictionary_manager_in_memory.h"
#include "services/network/shared_dictionary/shared_dictionary_manager_on_disk.h"
#include "services/network/shared_dictionary/shared_dictionary_storage.h"

namespace network {

namespace {

// SharedDictionaryManager keeps 10 instances in cache until there is memory
// pressure.
constexpr size_t kCachedStorageMaxSize = 10;

}  // namespace

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
    disk_cache::ApplicationStatusListenerGetter app_status_listener_getter,
#endif  // BUILDFLAG(IS_ANDROID)
    scoped_refptr<disk_cache::BackendFileOperationsFactory>
        file_operations_factory) {
  return std::make_unique<SharedDictionaryManagerOnDisk>(
      database_path, cache_directory_path, cache_max_size, cache_max_count,
#if BUILDFLAG(IS_ANDROID)
      app_status_listener_getter,
#endif  // BUILDFLAG(IS_ANDROID)
      std::move(file_operations_factory));
}

SharedDictionaryManager::SharedDictionaryManager()
    : cached_storages_(kCachedStorageMaxSize) {
  memory_pressure_listener_ = std::make_unique<base::MemoryPressureListener>(
      FROM_HERE, base::BindRepeating(&SharedDictionaryManager::OnMemoryPressure,
                                     weak_factory_.GetWeakPtr()));
}
SharedDictionaryManager::~SharedDictionaryManager() = default;

scoped_refptr<SharedDictionaryStorage> SharedDictionaryManager::GetStorage(
    const net::SharedDictionaryIsolationKey& isolation_key) {
  auto cached_storages_it = cached_storages_.Get(isolation_key);
  if (cached_storages_it != cached_storages_.end()) {
    return cached_storages_it->second;
  }

  auto it = storages_.find(isolation_key);
  if (it != storages_.end()) {
    DCHECK(it->second);
    return it->second.get();
  }
  scoped_refptr<SharedDictionaryStorage> storage = CreateStorage(isolation_key);
  CHECK(storage);
  storages_.emplace(isolation_key, storage.get());
  if (memory_pressure_level_ ==
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE) {
    cached_storages_.Put(isolation_key, storage);
  }
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

void SharedDictionaryManager::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel level) {
  memory_pressure_level_ = level;
  if (memory_pressure_level_ !=
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE) {
    cached_storages_.Clear();
  }
}

size_t SharedDictionaryManager::GetStorageCountForTesting() {
  return storages_.size();
}

}  // namespace network

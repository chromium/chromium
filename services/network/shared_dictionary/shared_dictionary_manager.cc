// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/shared_dictionary/shared_dictionary_manager.h"

#include <algorithm>
#include <memory>
#include <ranges>

#include "base/feature_list.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/trace_event/typed_macros.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/load_flags.h"
#include "net/shared_dictionary/shared_dictionary.h"
#include "services/network/public/cpp/features.h"
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

class SharedDictionaryManager::PreloadedDictionaries
    : public mojom::PreloadedSharedDictionaryInfoHandle {
 public:
  PreloadedDictionaries(
      mojo::PendingReceiver<mojom::PreloadedSharedDictionaryInfoHandle>
          preload_handle,
      SharedDictionaryManager* manager)
      : receiver_(this, std::move(preload_handle)), manager_(manager) {
    receiver_.set_disconnect_handler(base::BindOnce(
        &PreloadedDictionaries::OnDisconnected, base::Unretained(this)));
  }
  ~PreloadedDictionaries() override = default;
  PreloadedDictionaries(const PreloadedDictionaries&) = delete;
  PreloadedDictionaries& operator=(const PreloadedDictionaries&) = delete;

  void MaybeAddPreload(const GURL& url, mojom::RequestDestination destination) {
    url::Origin frame_origin = url::Origin::Create(url);
    if (frame_origin.opaque()) {
      return;
    }
    net::SharedDictionaryIsolationKey isolation_key(frame_origin,
                                                    net::SchemefulSite(url));
    scoped_refptr<SharedDictionaryStorage> storage =
        manager_->GetStorage(isolation_key);
    storages_.insert(storage);
    storage->GetDictionary(url, destination,
                           base::BindOnce(&PreloadedDictionaries::OnDictionary,
                                          weak_factory_.GetWeakPtr()));
  }

  bool Contains(const net::SharedDictionary& other) {
    return std::ranges::any_of(
        dictionaries_,
        [&other](const scoped_refptr<net::SharedDictionary>& dict) {
          return dict.get() == &other;
        });
  }

 private:
  void OnDisconnected() { manager_->DeletePreloadedDictionaries(this); }
  void OnDictionary(scoped_refptr<net::SharedDictionary> dictionary) {
    if (dictionary && !Contains(*dictionary)) {
      dictionaries_.insert(std::move(dictionary));
    }
  }
  std::set<scoped_refptr<SharedDictionaryStorage>> storages_;
  std::set<scoped_refptr<net::SharedDictionary>> dictionaries_;
  mojo::Receiver<mojom::PreloadedSharedDictionaryInfoHandle> receiver_;
  raw_ptr<SharedDictionaryManager> manager_;
  base::WeakPtrFactory<PreloadedDictionaries> weak_factory_{this};
};

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
  TRACE_EVENT("loading", "SharedDictionaryManager::GetStorage");
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
    preloaded_dictionaries_set_.clear();
  }
}

size_t SharedDictionaryManager::GetStorageCountForTesting() {
  return storages_.size();
}

net::SharedDictionaryGetter
SharedDictionaryManager::MaybeCreateSharedDictionaryGetter(
    int request_load_flags,
    mojom::RequestDestination request_destination) {
  if (!(request_load_flags & net::LOAD_CAN_USE_SHARED_DICTIONARY)) {
    return net::SharedDictionaryGetter();
  }
  return base::BindRepeating(
      [](base::WeakPtr<SharedDictionaryManager> manager,
         mojom::RequestDestination request_destination,
         const std::optional<net::SharedDictionaryIsolationKey>& isolation_key,
         const GURL& request_url) -> scoped_refptr<net::SharedDictionary> {
        return manager ? manager->GetDictionaryImpl(request_destination,
                                                    isolation_key, request_url)
                       : nullptr;
      },
      GetWeakPtr(), request_destination);
}

scoped_refptr<net::SharedDictionary> SharedDictionaryManager::GetDictionaryImpl(
    mojom::RequestDestination request_destination,
    const std::optional<net::SharedDictionaryIsolationKey>& isolation_key,
    const GURL& request_url) {
  if (!isolation_key) {
    return nullptr;
  }
  scoped_refptr<net::SharedDictionary> dict =
      GetStorage(*isolation_key)
          ->GetDictionarySync(request_url, request_destination);

  // Disable preloaded dictionary usage if the PreloadedDictionaryConditionalUse
  // feature is enabled and its binary is not yet loaded.
  if (dict &&
      base::FeatureList::IsEnabled(
          features::kPreloadedDictionaryConditionalUse) &&
      std::ranges::any_of(preloaded_dictionaries_set_,
                          [&dict](const auto& preloaded_dict) {
                            return preloaded_dict->Contains(*dict);
                          }) &&
      dict->ReadAll(base::BindOnce([](int) {})) != net::OK) {
    return nullptr;
  }
  return dict;
}

void SharedDictionaryManager::PreloadSharedDictionaryInfoForDocument(
    const std::vector<GURL>& urls,
    mojo::PendingReceiver<mojom::PreloadedSharedDictionaryInfoHandle>
        preload_handle) {
  if (memory_pressure_level_ !=
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE) {
    return;
  }
  auto preloaded_dictionaries =
      std::make_unique<PreloadedDictionaries>(std::move(preload_handle), this);
  for (const GURL& url : urls) {
    preloaded_dictionaries->MaybeAddPreload(
        url, mojom::RequestDestination::kDocument);
  }
  preloaded_dictionaries_set_.insert(std::move(preloaded_dictionaries));
}

void SharedDictionaryManager::DeletePreloadedDictionaries(
    PreloadedDictionaries* preloaded_dictionaries) {
  auto it = preloaded_dictionaries_set_.find(preloaded_dictionaries);
  CHECK(it != preloaded_dictionaries_set_.end());
  preloaded_dictionaries_set_.erase(it);
}

bool SharedDictionaryManager::HasPreloadedSharedDictionaryInfo() const {
  return !preloaded_dictionaries_set_.empty();
}

}  // namespace network

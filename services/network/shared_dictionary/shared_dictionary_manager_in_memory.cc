// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/shared_dictionary/shared_dictionary_manager_in_memory.h"

#include <algorithm>

#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "services/network/shared_dictionary/shared_dictionary_storage_in_memory.h"

namespace network {

namespace {

class DictionaryReference {
 public:
  DictionaryReference(
      raw_ptr<SharedDictionaryStorageInMemory> storage,
      raw_ptr<const SharedDictionaryStorageInMemory::DictionaryInfo> dict)
      : storage_(storage), dict_(dict) {}

  DictionaryReference(const DictionaryReference&) = default;
  DictionaryReference& operator=(const DictionaryReference&) = default;
  DictionaryReference(DictionaryReference&& other) = default;
  DictionaryReference& operator=(DictionaryReference&& other) = default;

  ~DictionaryReference() = default;

  raw_ptr<SharedDictionaryStorageInMemory> storage() const { return storage_; }
  const raw_ptr<const SharedDictionaryStorageInMemory::DictionaryInfo> dict()
      const {
    return dict_;
  }

 private:
  raw_ptr<SharedDictionaryStorageInMemory> storage_;
  raw_ptr<const SharedDictionaryStorageInMemory::DictionaryInfo> dict_;
};

struct LastUsedTimeLess {
  bool operator()(const DictionaryReference& a,
                  const DictionaryReference& b) const noexcept {
    return a.dict()->last_used_time() < b.dict()->last_used_time();
  }
};

class EvictionCandidate {
 public:
  EvictionCandidate(raw_ptr<SharedDictionaryStorageInMemory> storage,
                    const url::SchemeHostPort& host,
                    const std::string& match)
      : storage_(storage), host_(host), match_(match) {}

  EvictionCandidate(const EvictionCandidate&) = default;
  EvictionCandidate& operator=(const EvictionCandidate&) = default;
  EvictionCandidate(EvictionCandidate&& other) = default;
  EvictionCandidate& operator=(EvictionCandidate&& other) = default;
  ~EvictionCandidate() = default;

  const url::SchemeHostPort& host() const { return host_; }
  raw_ptr<SharedDictionaryStorageInMemory> storage() const { return storage_; }
  const std::string& match() const { return match_; }

 private:
  raw_ptr<SharedDictionaryStorageInMemory> storage_;
  url::SchemeHostPort host_;
  std::string match_;
};

}  // namespace

SharedDictionaryManagerInMemory::SharedDictionaryManagerInMemory(
    uint64_t cache_max_size)
    : cache_max_size_(cache_max_size) {}

SharedDictionaryManagerInMemory::~SharedDictionaryManagerInMemory() = default;

scoped_refptr<SharedDictionaryStorage>
SharedDictionaryManagerInMemory::CreateStorage(
    const net::SharedDictionaryStorageIsolationKey& isolation_key) {
  return base::MakeRefCounted<SharedDictionaryStorageInMemory>(
      weak_factory_.GetWeakPtr(),
      base::ScopedClosureRunner(
          base::BindOnce(&SharedDictionaryManager::OnStorageDeleted,
                         GetWeakPtr(), isolation_key)));
}

void SharedDictionaryManagerInMemory::SetCacheMaxSize(uint64_t cache_max_size) {
  cache_max_size_ = cache_max_size;
  MaybeRunCacheEviction();
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

void SharedDictionaryManagerInMemory::MaybeRunCacheEviction() {
  if (cache_max_size_ == 0u) {
    return;
  }
  uint64_t total_size = 0u;
  size_t dictionary_count = 0u;
  for (const auto& it1 : storages()) {
    SharedDictionaryStorageInMemory* storage =
        reinterpret_cast<SharedDictionaryStorageInMemory*>(it1.second.get());
    for (const auto& it2 : storage->GetDictionaryMap()) {
      dictionary_count += it2.second.size();
      for (const auto& it3 : it2.second) {
        total_size += it3.second.size();
      }
    }
  }
  if (total_size <= cache_max_size_) {
    return;
  }

  std::vector<DictionaryReference> dictionaries;
  dictionaries.reserve(dictionary_count);
  for (auto& it1 : storages()) {
    SharedDictionaryStorageInMemory* storage =
        reinterpret_cast<SharedDictionaryStorageInMemory*>(it1.second.get());
    for (auto& it2 : storage->GetDictionaryMap()) {
      for (auto& it3 : it2.second) {
        dictionaries.emplace_back(storage, &it3.second);
      }
    }
  }

  std::sort(dictionaries.begin(), dictionaries.end(), LastUsedTimeLess{});

  uint64_t low_watermark = cache_max_size_ * 0.9;
  std::vector<EvictionCandidate> eviction_candidates;
  for (auto& dict_ref : dictionaries) {
    total_size -= dict_ref.dict()->size();
    eviction_candidates.emplace_back(
        dict_ref.storage(), url::SchemeHostPort(dict_ref.dict()->url()),
        dict_ref.dict()->match());
    if (low_watermark >= total_size) {
      break;
    }
  }
  for (auto& candidate : eviction_candidates) {
    candidate.storage()->DeleteDictionary(candidate.host(), candidate.match());
  }
}

}  // namespace network

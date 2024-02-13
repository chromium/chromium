// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/shared_dictionary/shared_dictionary_manager_in_memory.h"

#include <algorithm>

#include "base/functional/callback_helpers.h"
#include "base/logging.h"
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
                    const std::string& match,
                    const std::set<mojom::RequestDestination>& match_dest)
      : storage_(storage),
        host_(host),
        match_(match),
        match_dest_(match_dest) {}

  EvictionCandidate(const EvictionCandidate&) = default;
  EvictionCandidate& operator=(const EvictionCandidate&) = default;
  EvictionCandidate(EvictionCandidate&& other) = default;
  EvictionCandidate& operator=(EvictionCandidate&& other) = default;
  ~EvictionCandidate() = default;

  const url::SchemeHostPort& host() const { return host_; }
  raw_ptr<SharedDictionaryStorageInMemory> storage() const { return storage_; }
  const std::string& match() const { return match_; }
  const std::set<mojom::RequestDestination> match_dest() const {
    return match_dest_;
  }

 private:
  raw_ptr<SharedDictionaryStorageInMemory> storage_;
  url::SchemeHostPort host_;
  std::string match_;
  std::set<mojom::RequestDestination> match_dest_;
};

}  // namespace

SharedDictionaryManagerInMemory::SharedDictionaryManagerInMemory(
    uint64_t cache_max_size,
    uint64_t cache_max_count)
    : cache_max_size_(cache_max_size), cache_max_count_(cache_max_count) {}

SharedDictionaryManagerInMemory::~SharedDictionaryManagerInMemory() = default;

scoped_refptr<SharedDictionaryStorage>
SharedDictionaryManagerInMemory::CreateStorage(
    const net::SharedDictionaryIsolationKey& isolation_key) {
  return base::MakeRefCounted<SharedDictionaryStorageInMemory>(
      weak_factory_.GetWeakPtr(), isolation_key,
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
  for (const auto& it : storages()) {
    SharedDictionaryStorageInMemory* storage =
        reinterpret_cast<SharedDictionaryStorageInMemory*>(it.second.get());
    base::RepeatingCallback<bool(const GURL&)> matcher = url_matcher;
    if (matcher && (matcher.Run(it.first.frame_origin().GetURL()) ||
                    matcher.Run(it.first.top_frame_site().GetURL()))) {
      matcher.Reset();
    }
    storage->ClearData(start_time, end_time, std::move(matcher));
  }
  std::move(callback).Run();
}

void SharedDictionaryManagerInMemory::ClearDataForIsolationKey(
    const net::SharedDictionaryIsolationKey& isolation_key,
    base::OnceClosure callback) {
  auto it = storages().find(isolation_key);
  if (it != storages().end()) {
    SharedDictionaryStorageInMemory* storage =
        reinterpret_cast<SharedDictionaryStorageInMemory*>(it->second.get());
    storage->ClearAllDictionaries();
  }
  std::move(callback).Run();
}

void SharedDictionaryManagerInMemory::MaybeRunCacheEvictionPerSite(
    const net::SchemefulSite& top_frame_site) {
  RunCacheEvictionImpl(top_frame_site, cache_max_size_ / 2, cache_max_size_ / 2,
                       cache_max_count_ / 2, cache_max_count_ / 2);
}

void SharedDictionaryManagerInMemory::MaybeRunCacheEviction() {
  RunCacheEvictionImpl(std::nullopt, cache_max_size_, cache_max_size_ * 0.9,
                       cache_max_count_, cache_max_count_ * 0.9);
}

void SharedDictionaryManagerInMemory::RunCacheEvictionImpl(
    std::optional<net::SchemefulSite> top_frame_site,
    uint64_t max_size,
    uint64_t size_low_watermark,
    uint64_t max_count,
    uint64_t count_low_watermark) {
  uint64_t total_size = 0u;
  size_t dictionary_count = 0u;
  for (const auto& it1 : storages()) {
    if (top_frame_site && it1.first.top_frame_site() != *top_frame_site) {
      continue;
    }
    SharedDictionaryStorageInMemory* storage =
        reinterpret_cast<SharedDictionaryStorageInMemory*>(it1.second.get());
    for (const auto& it2 : storage->GetDictionaryMap()) {
      dictionary_count += it2.second.size();
      for (const auto& it3 : it2.second) {
        total_size += it3.second.size();
      }
    }
  }

  if ((max_size == 0 || total_size <= max_size) &&
      dictionary_count <= max_count) {
    return;
  }

  std::vector<DictionaryReference> dictionaries;
  dictionaries.reserve(dictionary_count);
  for (auto& it1 : storages()) {
    if (top_frame_site && it1.first.top_frame_site() != *top_frame_site) {
      continue;
    }
    SharedDictionaryStorageInMemory* storage =
        reinterpret_cast<SharedDictionaryStorageInMemory*>(it1.second.get());
    for (auto& it2 : storage->GetDictionaryMap()) {
      for (auto& it3 : it2.second) {
        dictionaries.emplace_back(storage, &it3.second);
      }
    }
  }

  std::sort(dictionaries.begin(), dictionaries.end(), LastUsedTimeLess{});

  uint64_t to_be_removed_count = 0;
  if (dictionary_count > count_low_watermark) {
    to_be_removed_count = dictionary_count - count_low_watermark;
  }

  std::vector<EvictionCandidate> eviction_candidates;
  for (auto& dict_ref : dictionaries) {
    total_size -= dict_ref.dict()->size();
    eviction_candidates.emplace_back(
        dict_ref.storage(), url::SchemeHostPort(dict_ref.dict()->url()),
        dict_ref.dict()->match(), dict_ref.dict()->match_dest());
    if ((max_size == 0 || size_low_watermark >= total_size) &&
        eviction_candidates.size() >= to_be_removed_count) {
      break;
    }
  }
  dictionaries.clear();  // Unneeded, and may ref. about-to-be-deleted things.
  for (auto& candidate : eviction_candidates) {
    candidate.storage()->DeleteDictionary(candidate.host(), candidate.match(),
                                          candidate.match_dest());
  }
}

void SharedDictionaryManagerInMemory::GetUsageInfo(
    base::OnceCallback<void(const std::vector<net::SharedDictionaryUsageInfo>&)>
        callback) {
  std::vector<net::SharedDictionaryUsageInfo> result;
  for (auto& it : storages()) {
    SharedDictionaryStorageInMemory* storage =
        reinterpret_cast<SharedDictionaryStorageInMemory*>(it.second.get());
    net::SharedDictionaryUsageInfo usage;

    for (const auto& it1 : storage->GetDictionaryMap()) {
      for (const auto& it2 : it1.second) {
        usage.total_size_bytes += it2.second.size();
      }
    }
    if (usage.total_size_bytes != 0) {
      usage.isolation_key = it.first;
      result.emplace_back(usage);
    }
  }
  std::move(callback).Run(std::move(result));
}

void SharedDictionaryManagerInMemory::GetSharedDictionaryInfo(
    const net::SharedDictionaryIsolationKey& isolation_key,
    base::OnceCallback<
        void(std::vector<network::mojom::SharedDictionaryInfoPtr>)> callback) {
  std::vector<network::mojom::SharedDictionaryInfoPtr> dictionaries;

  const auto it = storages().find(isolation_key);
  if (it == storages().end()) {
    std::move(callback).Run(std::move(dictionaries));
    return;
  }
  SharedDictionaryStorageInMemory* storage =
      reinterpret_cast<SharedDictionaryStorageInMemory*>(it->second.get());

  std::vector<network::mojom::SharedDictionaryInfoPtr> dicts;
  for (const auto& it1 : storage->GetDictionaryMap()) {
    for (const auto& it2 : it1.second) {
      dictionaries.push_back(ToMojoSharedDictionaryInfo(it2.second));
    }
  }
  std::move(callback).Run(std::move(dictionaries));
}

void SharedDictionaryManagerInMemory::GetOriginsBetween(
    base::Time start_time,
    base::Time end_time,
    base::OnceCallback<void(const std::vector<url::Origin>&)> callback) {
  std::set<url::Origin> origins;
  for (const auto& it : storages()) {
    SharedDictionaryStorageInMemory* storage =
        reinterpret_cast<SharedDictionaryStorageInMemory*>(it.second.get());
    if (storage->HasDictionaryBetween(start_time, end_time)) {
      origins.insert(it.first.frame_origin());
    }
  }
  std::move(callback).Run(
      std::vector<url::Origin>(origins.begin(), origins.end()));
}

}  // namespace network

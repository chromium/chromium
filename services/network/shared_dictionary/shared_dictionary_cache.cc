// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/shared_dictionary/shared_dictionary_cache.h"

#include "base/feature_list.h"
#include "base/memory/ref_counted.h"
#include "net/shared_dictionary/shared_dictionary.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/request_destination.h"

namespace network {

SharedDictionaryCache::SharedDictionaryCache()
    : cache_(features::kSharedDictionaryCacheSize.Get()) {}

SharedDictionaryCache::~SharedDictionaryCache() = default;

scoped_refptr<net::SharedDictionary> SharedDictionaryCache::Get(
    const base::UnguessableToken& cache_key) {
  auto it = cache_.Get(cache_key);
  if (it != cache_.end()) {
    return it->second.get();
  }
  return nullptr;
}

void SharedDictionaryCache::Put(
    const base::UnguessableToken& cache_key,
    mojom::RequestDestination destination,
    scoped_refptr<net::SharedDictionary> dictionary) {
  if (base::FeatureList::IsEnabled(features::kSharedDictionaryCache) &&
      dictionary->size() <=
          features::kSharedDictionaryCacheMaxSizeBytes.Get() &&
      (destination == mojom::RequestDestination::kDocument ||
       destination == mojom::RequestDestination::kFrame ||
       destination == mojom::RequestDestination::kIframe)) {
    cache_.Put(cache_key, dictionary);
  }
}

void SharedDictionaryCache::Clear() {
  cache_.Clear();
}

}  // namespace network

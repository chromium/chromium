// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_CACHE_H_
#define SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_CACHE_H_

#include "base/component_export.h"
#include "base/containers/lru_cache.h"
#include "base/memory/ref_counted.h"
#include "base/unguessable_token.h"
#include "services/network/public/cpp/request_destination.h"

namespace net {
class SharedDictionary;
}  // namespace net

namespace network {

// This class is a ref-counted LRU memory cache for storing SharedDictionary
// instances. This is currently limited to document-like requests which tend to
// be used several times in a session but usually not simultaneously.
// TODO (crbug.com/411711704): Explore options to include non-document
// requests.
class COMPONENT_EXPORT(NETWORK_SERVICE) SharedDictionaryCache
    : public base::RefCounted<SharedDictionaryCache> {
 public:
  SharedDictionaryCache();

  SharedDictionaryCache(const SharedDictionaryCache&) = delete;
  SharedDictionaryCache& operator=(const SharedDictionaryCache&) = delete;

  scoped_refptr<net::SharedDictionary> Get(
      const base::UnguessableToken& cache_key);
  void Put(const base::UnguessableToken& cache_key,
           mojom::RequestDestination destination,
           scoped_refptr<net::SharedDictionary> dictionary);
  void Clear();

 protected:
  friend class base::RefCounted<SharedDictionaryCache>;
  virtual ~SharedDictionaryCache();

 private:
  base::LRUCache<base::UnguessableToken, scoped_refptr<net::SharedDictionary>>
      cache_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_CACHE_H_

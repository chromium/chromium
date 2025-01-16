// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_NO_VARY_SEARCH_CACHE_H_
#define NET_HTTP_NO_VARY_SEARCH_CACHE_H_

#include <stddef.h>

#include <compare>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include "base/containers/linked_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/stack_allocated.h"
#include "base/memory/weak_ptr.h"
#include "net/base/net_export.h"
#include "net/base/network_isolation_key.h"
#include "net/http/http_no_vary_search_data.h"
#include "url/gurl.h"

namespace net {

class HttpResponseHeaders;

// An in-memory cache that permits looking up a {NIK, URL} pair and seeing if it
// matches a previous response according to the rules of the No-Vary-Search
// header (see
// https://httpwg.org/http-extensions/draft-ietf-httpbis-no-vary-search.html).
// See also the design doc at
// https://docs.google.com/document/d/1RS3q6qZ7-k9CvZsDYseGOXzcdQ9fGZ6YYnaW7fTPu7A/edit
//
// Owned by net::HttpCache.
//
// Ignoring eviction, the data structure is approximately equivalent to
// std::map<std::tuple<NetworkIsolationKey, GURL, HttpNoVarySearchData>,
// QueryString>.
class NET_EXPORT_PRIVATE NoVarySearchCache {
 private:
  // Declared here so that it can be mentioned in the definition of EraseHandle.
  class QueryString;

 public:
  // Opaque object that permits erasure of an item from the cache.
  // See comments on the Lookup() and Erase() methods for usage.
  class NET_EXPORT_PRIVATE EraseHandle {
   public:
    ~EraseHandle();

    // Not copyable.
    EraseHandle(const EraseHandle&) = delete;
    EraseHandle& operator=(const EraseHandle&) = delete;

    // Movable.
    EraseHandle(EraseHandle&& rhs);
    EraseHandle& operator=(EraseHandle&& rhs);

    // For unit tests it is useful to be able to inspect this.
    bool EqualsForTesting(const EraseHandle& rhs) const;
    bool IsGoneForTesting() const;

   private:
    friend class NoVarySearchCache;
    friend class QueryString;

    explicit EraseHandle(base::WeakPtr<QueryString> query_string);

    base::WeakPtr<QueryString> query_string_;
  };

  struct LookupResult {
    GURL original_url;
    EraseHandle erase_handle;
  };

  // The cache will hold at most `max_size` entries. Each entry stores the query
  // parameter from a previous response, which will typically be 100 to 200
  // bytes.
  explicit NoVarySearchCache(size_t max_size);

  // Not copyable, assignable or movable.
  NoVarySearchCache(const NoVarySearchCache&) = delete;
  NoVarySearchCache& operator=(const NoVarySearchCache&) = delete;

  ~NoVarySearchCache();

  // Finds an entry in the cache equivalent to `url`. If a result is returned,
  // then `original_url` can be used to find a disk cache entry. `erase_handle`
  // can be used to remove the entry from this cache if it was not in the disk
  // cache. Not const because it updates the LRU linked list to mark the entry
  // as recently used.
  std::optional<LookupResult> Lookup(const NetworkIsolationKey& nik,
                                     const GURL& url);

  // Inserts `url` into the cache if a non-default "No-Vary-Search" header was
  // found in `headers`. On insertion, will remove any existing matching entry
  // with the same No-Vary-Search header, as the older entry would never be
  // returned by Lookup() anyway. May evict the oldest entry in the cache to
  // avoid the size exceeding `max_size_`.
  void MaybeInsert(const NetworkIsolationKey& nik,
                   const GURL& url,
                   const HttpResponseHeaders& headers);

  // Erases the entry referenced by `erase_handle` from the cache. Does nothing
  // if the entry no longer exists.
  void Erase(EraseHandle handle);

  // Returns the size (number of stored original query strings) of the cache.
  size_t GetSizeForTesting() const;

  // Returns true if the top-level map is empty. This should be equivalent to
  // GetSizeForTesting() == 0 in the absence of bugs.
  bool IsTopLevelMapEmptyForTesting() const;

 private:
  class LruNode;
  class QueryStringListNode;

  struct Key {
    NetworkIsolationKey nik;
    GURL base_url;

    bool operator==(const Key& rhs) const;
  };

  // KeyReference allows us to do a map lookup without constructing a GURL and a
  // Key.
  using KeyReference = std::pair<const NetworkIsolationKey&, std::string_view>;

  struct KeyComparator {
    using is_transparent = void;

    bool operator()(const Key& lhs, const Key& rhs) const;
    bool operator()(const Key& lhs, const KeyReference& rhs) const;
    bool operator()(const KeyReference& lhs, const Key& rhs) const;
  };

  friend bool operator==(const Key& lhs, const KeyReference& rhs) {
    return lhs.nik == rhs.first && lhs.base_url == rhs.second;
  }

  struct QueryStringList {
    base::LinkedList<QueryStringListNode> list;
    // nvs_data_ref can't be raw_ref because it needs to be lazily initialized
    // after the QueryStringList has been added to the map.
    raw_ptr<const HttpNoVarySearchData> nvs_data_ref;
    raw_ref<const Key> key_ref;

    explicit QueryStringList(const Key& key);
    // base::LinkedList<> does not do memory management, so make sure the
    // constents of `list` are deleted on destruction.
    ~QueryStringList();
  };

  struct FindQueryStringResult {
    STACK_ALLOCATED();  // `match` doesn't need to be raw_ptr.

   public:
    QueryString* match;
    GURL original_url;
  };

  // TODO(crbug.com/382394774): Investigate performance of different map types.
  using DataMapType = std::map<HttpNoVarySearchData, QueryStringList>;
  using OuterMapType = std::map<Key, DataMapType, KeyComparator>;

  // Erases an entry from the cache if it is full;
  void EvictIfFull();

  // Erases `query_string` from the cache.
  void EraseQuery(QueryString* query_string);

  static std::optional<FindQueryStringResult> FindQueryStringInList(
      QueryStringList& query_strings,
      std::string_view base,
      const GURL& url,
      const HttpNoVarySearchData& nvs_data);

  // The main cache data structure.
  OuterMapType map_;

  // lru_.tail() is the least-recently-used QueryString.
  base::LinkedList<LruNode> lru_;

  // The number of QueryString objects in the cache.
  size_t size_ = 0u;

  // QueryString objects will be evicted to avoid exceeding `max_size_`.
  const size_t max_size_;
};

}  // namespace net

#endif  // NET_HTTP_NO_VARY_SEARCH_CACHE_H_

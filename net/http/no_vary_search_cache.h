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
#include "base/types/strong_alias.h"
#include "net/base/net_export.h"
#include "net/http/http_no_vary_search_data.h"
#include "net/http/http_request_info.h"
#include "url/gurl.h"

namespace net {

class HttpResponseHeaders;

// An in-memory cache that permits looking up a URL and seeing if it matches a
// previous response according to the rules of the No-Vary-Search header (see
// https://httpwg.org/http-extensions/draft-ietf-httpbis-no-vary-search.html).
// See also the design doc at
// https://docs.google.com/document/d/1RS3q6qZ7-k9CvZsDYseGOXzcdQ9fGZ6YYnaW7fTPu7A/edit
//
// Owned by net::HttpCache.
//
// Ignoring eviction, the data structure is approximately equivalent to
// std::map<std::pair<BaseURLCacheKey, HttpNoVarySearchData>,
//          std::list<QueryString>>.
//
// BaseURLCacheKey is the output of the HttpCache key algorithm run on the base
// URL (everything before the "?"). So it incorporates the NetworkIsolationKey
// when split cache is enabled.
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

  // Finds an entry in the cache equivalent to `request.url` and in the same
  // cache partition. If a result is returned, then `original_url` can be used
  // to find a disk cache entry. `erase_handle` can be used to remove the entry
  // from this cache if it was not in the disk cache. Not const because it
  // updates the LRU linked list to mark the entry as recently used.
  std::optional<LookupResult> Lookup(const HttpRequestInfo& request);

  // Inserts `url` into the cache if a non-default "No-Vary-Search" header was
  // found in `headers`. On insertion, will remove any existing matching entry
  // with the same No-Vary-Search header, as the older entry would never be
  // returned by Lookup() anyway. May evict the oldest entry in the cache to
  // avoid the size exceeding `max_size_`.
  void MaybeInsert(const HttpRequestInfo& request,
                   const HttpResponseHeaders& headers);

  // TODO(https://crbug.com/382394774): Implement ClearData() so that entries
  // removed via the UI or Clear-Site-Data from the disk cache are also
  // removed from this cache. This is needed before persistence is implemented.
  // void ClearData(base::RepeatingCallback<bool(const GURL&)> url_matcher,
  //                base::Time delete_begin,
  //                base::Time delete_end);

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

  using BaseURLCacheKey =
      base::StrongAlias<struct BaseURLCacheKeyTagType, std::string>;

  struct QueryStringList {
    base::LinkedList<QueryStringListNode> list;
    // nvs_data_ref can't be raw_ref because it needs to be lazily initialized
    // after the QueryStringList has been added to the map.
    raw_ptr<const HttpNoVarySearchData> nvs_data_ref;
    raw_ref<const BaseURLCacheKey> key_ref;

    // The referent of this reference has to be the actual key in the map. It is
    // not sufficient for the value to match, because the lifetime has to be the
    // same.
    explicit QueryStringList(const BaseURLCacheKey& key);
    // base::LinkedList<> does not do memory management, so make sure the
    // contents of `list` are deleted on destruction.
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
  using OuterMapType = std::map<BaseURLCacheKey, DataMapType, std::less<>>;

  // Erases an entry from the cache if `size_ > max_size_`.
  void EvictIfOverfull();

  // Erases `query_string` from the cache.
  void EraseQuery(QueryString* query_string);

  static std::optional<FindQueryStringResult> FindQueryStringInList(
      QueryStringList& query_strings,
      const GURL& base,
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

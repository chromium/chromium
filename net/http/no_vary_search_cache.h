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
#include <vector>

#include "base/containers/linked_list.h"
#include "base/functional/function_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/stack_allocated.h"
#include "base/memory/weak_ptr.h"
#include "base/types/strong_alias.h"
#include "net/base/does_url_match_filter.h"
#include "net/base/net_export.h"
#include "net/base/pickle_traits.h"
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

  // An interface for receiving notifications about changes to the
  // NoVarySearchCache. Only insertions and refreshes via MaybeInsert() and
  // erasures via Erase() are reported to this interface. Evictions are
  // implicit, and modifications via ClearData() are expected to be followed by
  // persisting a fresh copy of the database.
  class NET_EXPORT_PRIVATE Journal {
   public:
    Journal() = default;

    Journal(const Journal&) = delete;
    Journal& operator=(const Journal&) = delete;

    // Called when an entry is inserted or refreshed by the MaybeInsert()
    // method. Not called when MaybeInsert() results in no changes to the
    // database. Also called by MergeFrom() for each merged entry.
    virtual void OnInsert(const std::string& base_url_cache_key,
                          const HttpNoVarySearchData& nvs_data,
                          const std::optional<std::string>& query,
                          base::Time update_time) = 0;

    // Called when an entry is erased by the Erase() method.
    virtual void OnErase(const std::string& base_url_cache_key,
                         const HttpNoVarySearchData& nvs_data,
                         const std::optional<std::string>& query) = 0;

   protected:
    // Journal objects are never deleted via a base class pointer.
    virtual ~Journal();
  };

  struct LookupResult {
    GURL original_url;
    EraseHandle erase_handle;
  };

  // The cache will hold at most `max_size` entries. Each entry stores the query
  // parameter from a previous response, which will typically be 100 to 200
  // bytes.
  explicit NoVarySearchCache(size_t max_size);

  // Move-constructible to permit deserialization and passing between threads.
  NoVarySearchCache(NoVarySearchCache&&);

  // Not copyable or assignable.
  NoVarySearchCache(const NoVarySearchCache&) = delete;
  NoVarySearchCache& operator=(const NoVarySearchCache&) = delete;
  NoVarySearchCache& operator=(NoVarySearchCache&&) = delete;

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

  // Synchronously deletes entries that match `origins` or `domains` with update
  // times equal or greater than `delete_begin` and less than `delete_end`.
  // Setting `filter_type` to UrlFilterType::kFalseIfMatching inverts the
  // meaning of `origins` and `domains` as with DoesURlMatchFilter(), but
  // doesn't affect the interpretation of `delete_begin` and `delete_end`.
  // In particular, ClearData(URLFilterType::kFalseIfMatching, {}, {},
  // base::Time(), base::Time::Max()) will delete everything. Returns `true` if
  // anything was removed.
  bool ClearData(UrlFilterType filter_type,
                 const base::flat_set<url::Origin>& origins,
                 const base::flat_set<std::string>& domains,
                 base::Time delete_begin,
                 base::Time delete_end);

  // Erases the entry referenced by `erase_handle` from the cache. Does
  // nothing if the entry no longer exists.
  void Erase(EraseHandle handle);

  // Set a Journal to be notified about subsequent changes to the cache. This
  // object does not take ownership of the Journal. Calling the method again
  // will replace the journal. The method can be called with nullptr to stop
  // being notified.
  void SetJournal(Journal* journal);

  // Adds the specified entry to the cache as if by MaybeInsert(), evicting an
  // older entry if the cache is full. The entry is treated as if newly used for
  // the purposes of eviction. For use when replaying journalled entries. The
  // arguments are expected to match a previous call to Journal::OnInsert()
  // from a different instance of NoVarySearchCache, but with the same settings
  // for cache partitioning. It can also be called with other valid arguments
  // for testing. If a valid base URL cannot be extracted from
  // `base_url_cache_key`, or `query` contains an invalid character, the call is
  // ignored. This will never happen if the arguments are unchanged from a call
  // to Journal::OnInsert() with the same partitioning. A valid base URL does
  // not contain a query or a fragment. Journal methods are not called.
  void ReplayInsert(std::string base_url_cache_key,
                    HttpNoVarySearchData nvs_data,
                    std::optional<std::string> query,
                    base::Time update_time);

  // Removes the specified entry from the cache as if by Erase(). For use when
  // replaying journalled entries. The arguments are expected to match a
  // previous call to Journal::OnErase from a different instance of
  // NoVarySearchCache, with the same settings for cache partitioning
  // base::Features. If `query` is not found the call silently
  // does nothing. Journal methods are not called.
  void ReplayErase(const std::string& base_url_cache_key,
                   const HttpNoVarySearchData& nvs_data,
                   const std::optional<std::string>& query);

  // Merge entries from `newer` in order from the least-recently-used to the
  // most-recently-used, treating them as newly used. Less recently-used entries
  // will be evicted if necessary to avoid exceeding the maximum size.
  // Journal::OnInsert() is called as if the entries were newly inserted (but
  // with the original update_time).
  void MergeFrom(const NoVarySearchCache& newer);

  // Returns the size (number of stored original query strings) of the cache.
  size_t size() const { return size_; }

  // Return the maximum size for the cache. Attempting to add more than this
  // many entries will result in older entries being evicted.
  size_t max_size() const { return max_size_; }

  // Returns true if the top-level map is empty. This should be equivalent to
  // size() == 0 in the absence of bugs.
  bool IsTopLevelMapEmptyForTesting() const;

 private:
  friend struct PickleTraits<NoVarySearchCache>;

  struct QueryStringList;
  friend struct PickleTraits<NoVarySearchCache::QueryStringList>;

  using BaseURLCacheKey =
      base::StrongAlias<struct BaseURLCacheKeyTagType, std::string>;
  friend struct PickleTraits<NoVarySearchCache::BaseURLCacheKey>;

  class LruNode;
  class QueryStringListNode;

  struct QueryStringList {
    base::LinkedList<QueryStringListNode> list;
    // nvs_data_ref can't be raw_ref because it needs to be lazily initialized
    // after the QueryStringList has been added to the map.
    raw_ptr<const HttpNoVarySearchData> nvs_data_ref = nullptr;

    // key_ref can't be raw_ref because it needs to be added in a second pass
    // during deserialization.
    raw_ptr<const BaseURLCacheKey> key_ref = nullptr;

    // The referent of this reference has to be the actual key in the map. It is
    // not sufficient for the value to match, because the lifetime has to be the
    // same.
    explicit QueryStringList(const BaseURLCacheKey& key);

    // Needed during deserialization.
    QueryStringList();

    // Only used during deserialization. This is O(N) in the size of `list`.
    QueryStringList(QueryStringList&&);

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

  // Inserts `query` or marks it as used in the cache. evicting an older entry
  // if necessary to make space. `journal` is notified if set.
  void DoInsert(const GURL& url,
                const GURL& base_url,
                std::string base_url_cache_key,
                HttpNoVarySearchData nvs_data,
                std::optional<std::string_view> query,
                base::Time update_time,
                Journal* journal);

  // A convenience method for callers that do not have the original URL handy.
  // Reconstructs the original URL and then calls DoInsert().
  void ReconstructURLAndDoInsert(const GURL& base_url,
                                 std::string base_url_cache_key,
                                 HttpNoVarySearchData nvs_data,
                                 std::optional<std::string> query,
                                 base::Time update_time,
                                 Journal* journal);

  // Scans all the QueryStrings in `data_map` to find ones in the range
  // [delete_begin, delete_end) and appends them to `matches`. `data_map` is
  // mutable to reflect that it is returning mutable pointers to QueryString
  // objects that it owns. The returned QueryString objects are mutable so the
  // caller can erase them.
  static void FindQueryStringsInTimeRange(DataMapType& data_map,
                                          base::Time delete_begin,
                                          base::Time delete_end,
                                          std::vector<QueryString*>& matches);

  static std::optional<FindQueryStringResult> FindQueryStringInList(
      QueryStringList& query_strings,
      const GURL& base,
      const GURL& url,
      const HttpNoVarySearchData& nvs_data);

  // Calls f(query_string_ptr) for every QueryString in `list`.
  static void ForEachQueryString(base::LinkedList<QueryStringListNode>& list,
                                 base::FunctionRef<void(QueryString*)> f);

  // Calls f(const_query_string_ptr) for every QueryString in `list`.
  static void ForEachQueryString(
      const base::LinkedList<QueryStringListNode>& list,
      base::FunctionRef<void(const QueryString*)> f);

  // The main cache data structure.
  OuterMapType map_;

  // lru_.tail() is the least-recently-used QueryString.
  base::LinkedList<LruNode> lru_;

  // The number of QueryString objects in the cache.
  size_t size_ = 0u;

  // QueryString objects will be evicted to avoid exceeding `max_size_`.
  const size_t max_size_;

  // An object to be notified about changes to this cache.
  raw_ptr<Journal> journal_ = nullptr;
};

template <>
struct NET_EXPORT_PRIVATE PickleTraits<NoVarySearchCache> {
  static void Serialize(base::Pickle& pickle, const NoVarySearchCache& cache);

  static std::optional<NoVarySearchCache> Deserialize(
      base::PickleIterator& iter);

  static size_t PickleSize(const NoVarySearchCache& cache);
};

}  // namespace net

#endif  // NET_HTTP_NO_VARY_SEARCH_CACHE_H_

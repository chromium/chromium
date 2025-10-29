// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_NO_VARY_SEARCH_CACHE_H_
#define NET_HTTP_NO_VARY_SEARCH_CACHE_H_

#include <stddef.h>

#include <concepts>
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
#include "net/base/does_url_match_filter.h"
#include "net/base/net_export.h"
#include "net/base/pickle_traits.h"
#include "net/http/http_no_vary_search_data.h"
#include "net/http/http_request_info.h"
#include "third_party/abseil-cpp/absl/container/node_hash_map.h"
#include "url/gurl.h"

namespace base {
class Time;
}

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
// std::map<std::tuple<CachePartitionKey, BaseURL, HttpNoVarySearchData,
//                     CanonicalizedQuery>,
//          std::unique_ptr<Query>>.
class NET_EXPORT_PRIVATE NoVarySearchCache {
 private:
  // Declared here so that it can be mentioned in the definition of EraseHandle.
  class Query;

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
    friend class Query;

    explicit EraseHandle(base::WeakPtr<Query> query_string);

    base::WeakPtr<Query> query_;
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
    virtual void OnInsert(const std::string& partition_key,
                          const std::string& base_url,
                          const HttpNoVarySearchData& nvs_data,
                          const std::optional<std::string>& query,
                          base::Time update_time) = 0;

    // Called when an entry is erased by the Erase() method.
    virtual void OnErase(const std::string& partition_key,
                         const std::string& base_url,
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
  std::optional<LookupResult> Lookup(const HttpRequestInfo& request,
                                     bool& out_base_url_matched);

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
  // arguments are expected to match a previous call to Journal::OnInsert() from
  // a different instance of NoVarySearchCache, but with the same settings for
  // cache partitioning. It can also be called with other valid arguments for
  // testing. If`base_url` is not a valid base URL, or `query` contains an
  // invalid character, the call is ignored. This will never happen if the
  // arguments are unchanged from a call to Journal::OnInsert() with the same
  // partitioning. A valid base URL does not contain a query or a fragment.
  // Journal methods are not called.
  void ReplayInsert(std::string partition_key,
                    std::string base_url,
                    HttpNoVarySearchData nvs_data,
                    std::optional<std::string> query,
                    base::Time update_time);

  // Removes the specified entry from the cache as if by Erase(). For use when
  // replaying journalled entries. The arguments are expected to match a
  // previous call to Journal::OnErase from a different instance of
  // NoVarySearchCache, with the same settings for cache partitioning
  // base::Features. If `query` is not found the call silently
  // does nothing. Journal methods are not called.
  void ReplayErase(const std::string& partition_key,
                   const std::string& base_url,
                   const HttpNoVarySearchData& nvs_data,
                   const std::optional<std::string>& query);

  // Merge entries from `newer` in order from the least-recently-used to the
  // most-recently-used, treating them as newly used. Less recently-used entries
  // will be evicted if necessary to avoid exceeding the maximum size.
  // Journal::OnInsert() is called as if the entries were newly inserted (but
  // with the original update_time).
  void MergeFrom(const NoVarySearchCache& newer);

  // Changes the maximum number of entries stored in the cache to the supplied
  // value, evicting entries if necessary to fit within the new limit. Optimized
  // for the case when `max_size_` doesn't change.
  void SetMaxSize(size_t max_size);

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
  friend struct PickleTraits<std::unique_ptr<NoVarySearchCache::Query>>;

  // All the maps use absl::node_hash_map rather than absl::flat_hash_map
  // because pointer stability for the keys is needed in order to be able to
  // erase a Query object by its pointer.
  using CanonicalizedQueryToQueryMap =
      absl::node_hash_map<std::string, std::unique_ptr<Query>>;
  friend struct PickleTraits<NoVarySearchCache::CanonicalizedQueryToQueryMap>;

  struct Queries {
    CanonicalizedQueryToQueryMap query_map;
    // In order to erase a Query from the cache, we need to be able to
    // find the map entries that contain it. To do this we have pointers back to
    // the keys.
    raw_ptr<const HttpNoVarySearchData> nvs_data_ptr = nullptr;
    raw_ptr<const std::string> base_url_ptr = nullptr;
    raw_ptr<const std::string> cache_partition_key_ptr = nullptr;

    Queries();
    ~Queries();

    // `query_map` is not copyable because the values are unique_ptrs.
    Queries(const Queries&) = delete;
    Queries& operator=(const Queries&) = delete;

    // It is movable.
    Queries(Queries&&);
    Queries& operator=(Queries&&) = default;
  };
  friend struct PickleTraits<NoVarySearchCache::Queries>;

  using NVSDataToQueriesMap =
      absl::node_hash_map<HttpNoVarySearchData, Queries>;
  using BaseUrlToNVSDataMap =
      absl::node_hash_map<std::string, NVSDataToQueriesMap>;
  using CachePartitionKeyToBaseUrlMap =
      absl::node_hash_map<std::string, BaseUrlToNVSDataMap>;

  // Erases an entry from the cache if `size_ > max_size_`.
  void EvictIfOverfull();

  // Erases `query_string` from the cache.
  void EraseQuery(Query* query_string);

  // Inserts `query` or marks it as used in the cache, evicting an older entry
  // if necessary to make space. `journal` is notified if set.
  void DoInsert(const GURL& url,
                std::string cache_partition_key,
                std::string base_url,
                HttpNoVarySearchData nvs_data,
                std::optional<std::string> query,
                base::Time update_time,
                Journal* journal);

  // A convenience method for callers that do not have the original URL handy.
  // Reconstructs the original URL and then calls DoInsert().
  void ReconstructURLAndDoInsert(std::string partition_key,
                                 std::string base_url,
                                 HttpNoVarySearchData nvs_data,
                                 std::optional<std::string> query,
                                 base::Time update_time,
                                 Journal* journal);

  // Scans all the Query objects in `data_map` to find ones in the range
  // [delete_begin, delete_end) and appends them to `matches`. `data_map` is
  // mutable to reflect that it is returning mutable pointers to Query objects
  // that it owns. The returned Query objects are mutable so the caller can
  // erase them.
  static void FindQuerysInTimeRange(NVSDataToQueriesMap& data_map,
                                    base::Time delete_begin,
                                    base::Time delete_end,
                                    std::vector<Query*>& matches);

  // Find a Query matching the query part of `url` based on the rules in
  // `nvs_data` if one exists, or returns nullptr.
  static Query* FindQuery(CanonicalizedQueryToQueryMap& query_map,
                          const GURL& url,
                          const HttpNoVarySearchData& nvs_data);

  // Calls f(query_string_ptr) for every Query in `queries`.
  static void ForEachQuery(Queries& queries, base::FunctionRef<void(Query*)> f);

  // The main cache data structure. The key is the cache partition key as
  // generated by HttpCache::GenerateCachePartitionKeyForRequest(). The value is
  // a map with base URL stringified as keys. The value of that map is another
  // map with NoVarySearchData objects as keys, and the value of that map is
  // another map with canonicalized query strings as keys. That map has Query
  // objects as keys.
  // CachePartitionKey (std::string) ->
  //   Base URL (std::string) ->
  //     NVS data (NoVarySearchData) ->
  //       Canonicalized query (std::string) -> Query object
  CachePartitionKeyToBaseUrlMap partitions_;

  // lru_.tail() is the least-recently-used Query.
  base::LinkedList<Query> lru_;

  // The number of Query objects in the cache.
  size_t size_ = 0u;

  // Query objects will be evicted to avoid exceeding `max_size_`.
  size_t max_size_;

  // An object to be notified about changes to this cache.
  raw_ptr<Journal> journal_ = nullptr;
};

// Specialization of PickleTraits needed for serializing and deserializing
// NoVarySearchCache objects.
template <>
struct NET_EXPORT_PRIVATE PickleTraits<NoVarySearchCache> {
  static void Serialize(base::Pickle& pickle, const NoVarySearchCache& cache);

  static std::optional<NoVarySearchCache> Deserialize(
      base::PickleIterator& iter);

  static size_t PickleSize(const NoVarySearchCache& cache);
};

}  // namespace net

#endif  // NET_HTTP_NO_VARY_SEARCH_CACHE_H_

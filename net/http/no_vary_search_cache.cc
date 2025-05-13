// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/no_vary_search_cache.h"

#include <algorithm>
#include <compare>
#include <iostream>
#include <limits>
#include <map>
#include <tuple>
#include <type_traits>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "net/base/pickle.h"
#include "net/base/pickle_base_types.h"
#include "net/http/http_cache.h"
#include "net/http/http_no_vary_search_data.h"

namespace net {

namespace {

// We need to use a separate enum for the
// HttpCache.NoVarySearch.HeaderParseResult than
// HttpNoVarySearchData::ParseErrorEnum, as that enum does not have a value for
// a successful parse.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(NoVarySearchHeaderParseResult)
enum class NoVarySearchHeaderParseResult {
  kSuccess = 0,
  kNoHeader = 1,
  kDefaultValue = 2,
  kNotDictionary = 3,
  kNonBooleanKeyOrder = 4,
  kParamsNotStringList = 5,
  kExceptNotStringList = 6,
  kExceptWithoutTrueParams = 7,
  kMaxValue = kExceptWithoutTrueParams,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/net/enums.xml:NoVarySearchHeaderParseResult)

NoVarySearchHeaderParseResult MapParseErrorEnum(
    HttpNoVarySearchData::ParseErrorEnum error) {
  using enum HttpNoVarySearchData::ParseErrorEnum;
  switch (error) {
    case kOk:
      return NoVarySearchHeaderParseResult::kNoHeader;

    case kDefaultValue:
      return NoVarySearchHeaderParseResult::kDefaultValue;

    case kNotDictionary:
      return NoVarySearchHeaderParseResult::kNotDictionary;

    case kUnknownDictionaryKey:
      NOTREACHED();  // No longer used.

    case kNonBooleanKeyOrder:
      return NoVarySearchHeaderParseResult::kNonBooleanKeyOrder;

    case kParamsNotStringList:
      return NoVarySearchHeaderParseResult::kParamsNotStringList;

    case kExceptNotStringList:
      return NoVarySearchHeaderParseResult::kExceptNotStringList;

    case kExceptWithoutTrueParams:
      return NoVarySearchHeaderParseResult::kExceptWithoutTrueParams;
  }
  NOTREACHED();
}

void EmitNoVarySearchHeaderParseResultHistogram(
    const base::expected<HttpNoVarySearchData,
                         HttpNoVarySearchData::ParseErrorEnum>& result) {
  auto value = NoVarySearchHeaderParseResult::kSuccess;
  if (!result.has_value()) {
    value = MapParseErrorEnum(result.error());
  }
  UMA_HISTOGRAM_ENUMERATION("HttpCache.NoVarySearch.HeaderParseResult", value);
}

// Stripping the URL of its query and fragment (ref) needs to be done for every
// request, so we want to avoid allocating memory for a GURL in the case of a
// cache miss. The return value points at memory owned by `url`, so should not
// outlive it.
GURL ExtractBaseURL(const GURL& url) {
  CHECK(url.is_valid());
  GURL::Replacements replacements;
  replacements.ClearQuery();
  replacements.ClearRef();
  return url.ReplaceComponents(replacements);
}

bool URLIsAcceptable(const GURL& url) {
  // HTTP(S) URLs always have a path starting with "/" after canonicalization.
  return url.is_valid() && url.has_path() && !url.has_username() &&
         !url.has_password();
}

bool BaseURLIsAcceptable(const GURL& base_url) {
  return URLIsAcceptable(base_url) && !base_url.has_query() &&
         !base_url.has_ref();
}

// Given `base_url` and `query`, return the original URL that would have been
// used to construct them.
GURL ReconstructOriginalURLFromQuery(const GURL& base_url,
                                     const std::optional<std::string>& query) {
  if (!query.has_value()) {
    return base_url;
  }

  GURL::Replacements replacements;
  replacements.SetQueryStr(query.value());
  return base_url.ReplaceComponents(replacements);
}

}  // namespace

// base::LinkedList only supports having an object in a single linked list at a
// time. However, we need QueryString objects to be in two separate lists:
//  * the least-recently-used (LRU) list, which has the most recently added or
//    used entry at its head and the next entry to be evicted at its tail. This
//    list contains every QueryString in the cache.
//  * the list of cached QueryString objects for a particular base URL and
//    No-Vary-Search parameter. These lists have the most recently inserted
//    entry for this {base URL, NVS} pair at their heads.
//
// In order to work-around the limitation of base::LinkedList, QueryString has
// two base classes. LruNode represents its role as a member of the LRU list,
// and QueryStringListNode represents its role as a member of the
// QueryStringList list. By calling base::LinkNode methods via the appropriate
// base class it can control which list it is manipulating.
class NoVarySearchCache::LruNode : public base::LinkNode<LruNode> {
 public:
  // Not copyable or movable.
  LruNode(const LruNode&) = delete;
  LruNode& operator=(const LruNode&) = delete;

  // Every instance of LruNode is actually a QueryString.
  QueryString* ToQueryString();

 private:
  friend class QueryString;

  LruNode() = default;
  ~LruNode() = default;
};

class NoVarySearchCache::QueryStringListNode
    : public base::LinkNode<QueryStringListNode> {
 public:
  // Not copyable or movable.
  QueryStringListNode(const QueryStringListNode&) = delete;
  QueryStringListNode& operator=(const QueryStringListNode&) = delete;

  // Every instance of QueryStringListNode is actually a QueryString.
  QueryString* ToQueryString();

 private:
  friend class QueryString;

  QueryStringListNode() = default;
  ~QueryStringListNode() = default;
};

// QueryString is the entry type for the cache. Its main purpose is to hold the
// query string, ie. everything between the "?" and the "#" in the original URL.
// Together with the `base_url`, this can be used to reconstruct the original
// URL that was used to store the original request in the disk cache.
class NoVarySearchCache::QueryString final
    : public NoVarySearchCache::LruNode,
      public NoVarySearchCache::QueryStringListNode {
 public:
  // Creates a QueryString and adds it to `list` and `lru_list`.
  static QueryString* CreateAndInsert(std::optional<std::string_view> query,
                                      QueryStringList& query_string_list,
                                      base::LinkedList<LruNode>& lru_list,
                                      base::Time update_time) {
    DCHECK(!query || query->find('#') == std::string_view::npos)
        << "Query contained a '#' character, meaning that the URL reassembly "
           "will not work correctly because the '#' will be re-interpreted as "
           "the start of a fragment. This should not happen. Query was '"
        << query.value() << "'";
    // This use of bare new is needed because base::LinkedList does not have
    // ownership semantics.
    auto* query_string = new QueryString(query, query_string_list, update_time);
    query_string->LruNode::InsertBefore(lru_list.head());
    query_string->QueryStringListNode::InsertBefore(
        query_string_list.list.head());
    return query_string;
  }

  // Not copyable or movable.
  QueryString(const QueryString&) = delete;
  QueryString& operator=(const QueryString&) = delete;

  // Removes this object from both lists and deletes it.
  void RemoveAndDelete() {
    // During deserialization a QueryString is not inserted into the `lru_` list
    // until the end. If deserialization fails before then, it can be deleted
    // without ever being inserted into the `lru_` list.
    if (LruNode::next()) {
      CHECK(LruNode::previous());
      LruNode::RemoveFromList();
    }
    QueryStringListNode::RemoveFromList();
    delete this;
  }

  // Moves this object to the head of `list`.
  template <typename List>
  void MoveToHead(List& linked_list) {
    auto* head = linked_list.head();
    if (head != this) {
      MoveBeforeNode(linked_list.head()->value());
    }
  }

  const std::optional<std::string>& query() const { return query_; }

  QueryStringList& query_string_list_ref() { return *query_string_list_ref_; }

  base::Time update_time() const { return update_time_; }

  void set_update_time(base::Time update_time) { update_time_ = update_time; }

  // Return the original GURL that this entry was constructed from (not
  // including any fragment). It's important to use this method to correctly
  // reconstruct URLs that have an empty query (end in '?').
  GURL ReconstructOriginalURL(const GURL& base_url) {
    return ReconstructOriginalURLFromQuery(base_url, query_);
  }

  EraseHandle CreateEraseHandle() {
    return EraseHandle(weak_factory_.GetWeakPtr());
  }

  void set_query_string_list_ref(
      base::PassKey<NoVarySearchCache::QueryStringList>,
      QueryStringList* query_string_list) {
    query_string_list_ref_ = query_string_list;
  }

 private:
  friend struct PickleTraits<NoVarySearchCache::QueryStringList>;

  QueryString(std::optional<std::string_view> query,
              QueryStringList& query_string_list,
              base::Time update_time)
      : query_(query),
        query_string_list_ref_(&query_string_list),
        update_time_(update_time) {}

  // Must only be called from RemoveAndDelete().
  ~QueryString() = default;

  // Moves this object in front of `node`.
  template <typename NodeType>
  void MoveBeforeNode(NodeType* node) {
    static_assert(std::same_as<NodeType, LruNode> ||
                      std::same_as<NodeType, QueryStringListNode>,
                  "This assert is just documentation");
    CHECK_NE(node, this);
    NodeType::RemoveFromList();
    NodeType::InsertBefore(node);
  }

  // No-Vary-Search treats "http://www.example.com/" and
  // "http://www.example.com/?" as the same URL, but the disk cache key treats
  // them as different URLs, so we need to be able to distinguish them to
  // correctly reconstruct the original URL. `query_ == std::nullopt` means that
  // there was no `?` in the original URL, and `query_ == ""` means there was.
  const std::optional<std::string> query_;

  // `query_string_list_ref_` allows the keys for this entry to be located in
  // the cache so that it can be erased efficiently. It is modified when a
  // QueryStringList object is moved.
  raw_ptr<QueryStringList> query_string_list_ref_ = nullptr;

  // `update_time_` breaks ties when there are multiple possible matches. The
  // most recent entry will be used as it is most likely to still exist in the
  // disk cache.
  base::Time update_time_;

  // EraseHandle uses weak pointers to QueryString objects to enable an entry to
  // be deleted from the cache if it is found not to be readable from the disk
  // cache.
  base::WeakPtrFactory<QueryString> weak_factory_{this};
};

// The two implementations of ToQueryString() are defined out-of-line so that
// the compiler has seen the definition of QueryString and so knows the correct
// offsets to apply to the `this` pointer.
NoVarySearchCache::QueryString* NoVarySearchCache::LruNode::ToQueryString() {
  return static_cast<QueryString*>(this);
}

NoVarySearchCache::QueryString*
NoVarySearchCache::QueryStringListNode::ToQueryString() {
  return static_cast<QueryString*>(this);
}

NoVarySearchCache::EraseHandle::EraseHandle(
    base::WeakPtr<QueryString> query_string)
    : query_string_(std::move(query_string)) {}

NoVarySearchCache::EraseHandle::~EraseHandle() = default;

NoVarySearchCache::EraseHandle::EraseHandle(EraseHandle&& rhs) = default;
NoVarySearchCache::EraseHandle& NoVarySearchCache::EraseHandle::operator=(
    EraseHandle&& rhs) = default;

bool NoVarySearchCache::EraseHandle::EqualsForTesting(
    const EraseHandle& rhs) const {
  return query_string_.get() == rhs.query_string_.get();
}

bool NoVarySearchCache::EraseHandle::IsGoneForTesting() const {
  return !query_string_;
}

NoVarySearchCache::Journal::~Journal() = default;

NoVarySearchCache::NoVarySearchCache(size_t max_size) : max_size_(max_size) {
  CHECK_GE(max_size_, 1u);
  // We can't serialize if `max_size` won't fit in an int.
  CHECK(base::IsValueInRangeForNumericType<int>(max_size));
}

NoVarySearchCache::NoVarySearchCache(NoVarySearchCache&& rhs)
    : map_(std::move(rhs.map_)),
      lru_(std::move(rhs.lru_)),
      size_(std::exchange(rhs.size_, 0u)),
      max_size_(rhs.max_size_) {}

NoVarySearchCache::~NoVarySearchCache() {
  map_.clear();
  // Clearing the map should have freed all the QueryString objects.
  CHECK(lru_.empty());
}

std::optional<NoVarySearchCache::LookupResult> NoVarySearchCache::Lookup(
    const HttpRequestInfo& request) {
  SCOPED_UMA_HISTOGRAM_TIMER_MICROS("HttpCache.NoVarySearch.LookupTime");
  const GURL& url = request.url;
  if (!URLIsAcceptable(url)) {
    return std::nullopt;
  }
  // TODO(https://crbug.com/388956603): Try to avoid allocating memory for the
  // base url.
  const GURL base_url = ExtractBaseURL(url);
  // TODO(https://crbug.com/388956603): This does a lot of allocations and
  // string copies. Try to reduce the amount of work done for a miss.
  const std::optional<std::string> maybe_cache_key =
      HttpCache::GenerateCacheKeyForRequestWithAlternateURL(&request, base_url);
  if (!maybe_cache_key) {
    return std::nullopt;
  }
  const BaseURLCacheKey cache_key(maybe_cache_key.value());
  const auto it = map_.find(cache_key);
  if (it == map_.end()) {
    return std::nullopt;
  }
  // We have a match, so we need to create a real URL now.
  QueryString* best_match = nullptr;
  GURL original_url;
  for (auto& [nvs_data, query_strings] : it->second) {
    auto result = FindQueryStringInList(query_strings, base_url, url, nvs_data);
    if (result && (!best_match ||
                   best_match->update_time() < result->match->update_time())) {
      best_match = result->match;
      original_url = result->original_url;
    }
  }
  if (!best_match) {
    return std::nullopt;
  }
  // Move to head of `lru_` list.
  best_match->MoveToHead(lru_);

  return LookupResult(original_url, best_match->CreateEraseHandle());
}

void NoVarySearchCache::MaybeInsert(const HttpRequestInfo& request,
                                    const HttpResponseHeaders& headers) {
  const GURL& url = request.url;
  if (!URLIsAcceptable(url)) {
    return;
  }
  auto maybe_nvs_data = HttpNoVarySearchData::ParseFromHeaders(headers);
  EmitNoVarySearchHeaderParseResultHistogram(maybe_nvs_data);
  if (!maybe_nvs_data.has_value()) {
    return;
  }
  const GURL base_url = ExtractBaseURL(url);

  std::optional<std::string_view> query;
  if (url.has_query()) {
    query = url.query_piece();
  }

  // Using lower_bound() followed by emplace_hint() allows us to avoid
  // constructing a Key object if there is already a matching key in the map,
  // and do only a single logN lookup.
  const std::optional<std::string> maybe_cache_key =
      HttpCache::GenerateCacheKeyForRequestWithAlternateURL(&request, base_url);
  if (!maybe_cache_key) {
    return;
  }

  const base::Time update_time = base::Time::Now();

  DoInsert(url, base_url, std::move(maybe_cache_key.value()),
           std::move(maybe_nvs_data.value()), query, update_time, journal_);
}

bool NoVarySearchCache::ClearData(UrlFilterType filter_type,
                                  const base::flat_set<url::Origin>& origins,
                                  const base::flat_set<std::string>& domains,
                                  base::Time delete_begin,
                                  base::Time delete_end) {
  // For simplicity, first collect a list of matching QueryStrings to erase and
  // then erase them.
  // TODO(https://crbug.com/382394774): Make this algorithm more efficient.
  std::vector<QueryString*> pending_erase;
  for (auto& [cache_key, data_map] : map_) {
    const std::string base_url_string =
        HttpCache::GetResourceURLFromHttpCacheKey(cache_key.value());
    const GURL base_url(base_url_string);
    CHECK(base_url.is_valid());
    // DoesUrlMatchFilter() only looks at the origin of the URL, which is why we
    // don't need to worry about reconstructing the full URL with query.
    if (DoesUrlMatchFilter(filter_type, origins, domains, base_url)) {
      FindQueryStringsInTimeRange(data_map, delete_begin, delete_end,
                                  pending_erase);
    }
  }
  for (QueryString* query_string : pending_erase) {
    EraseQuery(query_string);
  }
  return !pending_erase.empty();
}

void NoVarySearchCache::Erase(EraseHandle handle) {
  if (QueryString* query_string = handle.query_string_.get()) {
    if (journal_) {
      auto& query_string_list = query_string->query_string_list_ref();
      journal_->OnErase(query_string_list.key_ref->value(),
                        *query_string_list.nvs_data_ref, query_string->query());
    }

    EraseQuery(query_string);
  }
}

void NoVarySearchCache::SetJournal(Journal* journal) {
  journal_ = journal;
}

void NoVarySearchCache::ReplayInsert(std::string base_url_cache_key,
                                     HttpNoVarySearchData nvs_data,
                                     std::optional<std::string> query,
                                     base::Time update_time) {
  const std::string base_url_string =
      HttpCache::GetResourceURLFromHttpCacheKey(base_url_cache_key);
  const GURL base_url(base_url_string);
  if (!BaseURLIsAcceptable(base_url)) {
    return;
  }
  // The URL should have been stored in its canonical form.
  if (base_url_string != base_url.possibly_invalid_spec()) {
    return;
  }
  if (query && query->find('#') != std::string::npos) {
    return;
  }

  // To be extra careful to avoid re-entrancy, explicitly set `journal` to
  // nullptr so that no notification is fired for this insertion.
  ReconstructURLAndDoInsert(base_url, std::move(base_url_cache_key),
                            std::move(nvs_data), std::move(query), update_time,
                            /*journal=*/nullptr);
}

void NoVarySearchCache::ReplayErase(const std::string& base_url_cache_key,
                                    const HttpNoVarySearchData& nvs_data,
                                    const std::optional<std::string>& query) {
  const auto map_it = map_.find(BaseURLCacheKey(base_url_cache_key));
  if (map_it == map_.end()) {
    return;
  }

  DataMapType& data_map = map_it->second;
  const auto data_it = data_map.find(nvs_data);
  if (data_it == data_map.end()) {
    return;
  }

  QueryStringList& query_strings = data_it->second;
  QueryString* query_string = nullptr;
  ForEachQueryString(query_strings.list,
                     [&query_string, &query](QueryString* candidate) {
                       if (!query_string && candidate->query() == query) {
                         query_string = candidate;
                       }
                     });
  if (!query_string) {
    return;
  }

  // TODO(https://crbug.com/382394774): This could be made more efficient in the
  // case when the map keys need to be deleted since we have `map_it` and
  // `data_it` already available.
  EraseQuery(query_string);
}

void NoVarySearchCache::MergeFrom(const NoVarySearchCache& newer) {
  // We cannot use ForEachQueryString() here as we need to iterate through the
  // `lru_` linked list in reverse order.
  const auto& newer_lru = newer.lru_;
  for (auto* node = newer_lru.tail(); node != newer_lru.end();
       node = node->previous()) {
    QueryString* query_string = node->value()->ToQueryString();
    const auto& query_string_list = query_string->query_string_list_ref();
    std::string base_url_cache_key(*query_string_list.key_ref);
    const HttpNoVarySearchData& nvs_data = *query_string_list.nvs_data_ref;
    const std::string base_url_string =
        HttpCache::GetResourceURLFromHttpCacheKey(base_url_cache_key);
    const GURL base_url(base_url_string);
    CHECK(BaseURLIsAcceptable(base_url));
    std::optional<std::string> query = query_string->query();
    CHECK(!query || query->find('#') == std::string::npos);

    // Pass `journal_` so the merged entries are journalled as insertions.
    ReconstructURLAndDoInsert(base_url, std::move(base_url_cache_key), nvs_data,
                              std::move(query), query_string->update_time(),
                              journal_);
  }
}

bool NoVarySearchCache::IsTopLevelMapEmptyForTesting() const {
  return map_.empty();
}

NoVarySearchCache::QueryStringList::QueryStringList(const BaseURLCacheKey& key)
    : key_ref(&key) {}

NoVarySearchCache::QueryStringList::QueryStringList() = default;

NoVarySearchCache::QueryStringList::QueryStringList(QueryStringList&& rhs)
    : list(std::move(rhs.list)) {
  // We should not move a list after the key references have been assigned.
  CHECK(!rhs.nvs_data_ref);
  CHECK(!rhs.key_ref);
  // We have to patch up all the references to `rhs` in our QueryString objects
  // to point to us instead.
  ForEachQueryString(list, [&](QueryString* query_string) {
    query_string->set_query_string_list_ref(base::PassKey<QueryStringList>(),
                                            this);
  });
}

NoVarySearchCache::QueryStringList::~QueryStringList() {
  while (!list.empty()) {
    list.head()->value()->ToQueryString()->RemoveAndDelete();
  }
}

void NoVarySearchCache::EvictIfOverfull() {
  CHECK_LE(size_, max_size_ + 1);
  if (size_ == max_size_ + 1) {
    // This happens when an entry is added when the cache is already full.
    // Remove an entry to make `size_` == `max_size_` again.
    EraseQuery(lru_.tail()->value()->ToQueryString());
  }
}

void NoVarySearchCache::EraseQuery(QueryString* query_string) {
  CHECK_GT(size_, 0u);
  --size_;
  const QueryStringList& query_strings = query_string->query_string_list_ref();
  query_string->RemoveAndDelete();
  if (query_strings.list.empty()) {
    const HttpNoVarySearchData& nvs_data_ref = *query_strings.nvs_data_ref;
    const BaseURLCacheKey& key_ref = *query_strings.key_ref;
    const auto map_it = map_.find(key_ref);
    CHECK(map_it != map_.end());
    const size_t removed_count = map_it->second.erase(nvs_data_ref);
    CHECK_EQ(removed_count, 1u);
    if (map_it->second.empty()) {
      map_.erase(map_it);
    }
  }
}

void NoVarySearchCache::DoInsert(const GURL& url,
                                 const GURL& base_url,
                                 std::string base_url_cache_key,
                                 HttpNoVarySearchData nvs_data,
                                 std::optional<std::string_view> query,
                                 base::Time update_time,
                                 Journal* journal) {
  const BaseURLCacheKey cache_key(std::move(base_url_cache_key));
  const auto [it, _] = map_.try_emplace(std::move(cache_key));
  const BaseURLCacheKey& cache_key_ref = it->first;
  DataMapType& data_map = it->second;
  const auto [data_it, inserted] =
      data_map.emplace(std::move(nvs_data), it->first);
  const HttpNoVarySearchData& nvs_data_ref = data_it->first;
  QueryStringList& query_strings = data_it->second;

  const auto call_journal = [journal, &cache_key_ref, &nvs_data_ref,
                             update_time](const QueryString* query_string) {
    if (journal) {
      journal->OnInsert(cache_key_ref.value(), nvs_data_ref,
                        query_string->query(), update_time);
    }
  };

  if (inserted) {
    query_strings.nvs_data_ref = &nvs_data_ref;
  } else {
    // There was already an entry for this `nvs_data`. We need to check if it
    // has a match for the URL we're trying to insert. If it does, we should
    // update or replace the existing QueryString.
    if (auto result =
            FindQueryStringInList(query_strings, base_url, url, nvs_data_ref)) {
      QueryString* match = result->match;
      if (match->query() == query) {
        // In the exact match case we can use the existing QueryString object.
        match->set_update_time(update_time);
        match->MoveToHead(query_strings.list);
        match->MoveToHead(lru_);
        call_journal(match);
        return;
      }

      // No-Vary-Search matches are transitive. Any future requests that might
      // be a match for `match` are also a match for `url`. Since `url` is newer
      // we will prefer it, and so `match` will never be used again and we can
      // safely remove it from the cache.
      --size_;
      match->RemoveAndDelete();
    }
  }
  CHECK_LE(size_, max_size_);
  ++size_;
  auto* query_string =
      QueryString::CreateAndInsert(query, query_strings, lru_, update_time);
  call_journal(query_string);
  EvictIfOverfull();
}

void NoVarySearchCache::ReconstructURLAndDoInsert(
    const GURL& base_url,
    std::string base_url_cache_key,
    HttpNoVarySearchData nvs_data,
    std::optional<std::string> query,
    base::Time update_time,
    Journal* journal) {
  const GURL url = ReconstructOriginalURLFromQuery(base_url, query);
  DoInsert(url, base_url, std::move(base_url_cache_key), std::move(nvs_data),
           std::move(query), update_time, journal);
}

// static
void NoVarySearchCache::FindQueryStringsInTimeRange(
    DataMapType& data_map,
    base::Time delete_begin,
    base::Time delete_end,
    std::vector<QueryString*>& matches) {
  for (auto& [_, query_string_list] : data_map) {
    ForEachQueryString(query_string_list.list, [&](QueryString* query_string) {
      const base::Time update_time = query_string->update_time();
      if ((delete_begin.is_null() || delete_begin <= update_time) &&
          (delete_end.is_max() || delete_end > update_time)) {
        matches.push_back(query_string);
      }
    });
  }
}

// static
std::optional<NoVarySearchCache::FindQueryStringResult>
NoVarySearchCache::FindQueryStringInList(QueryStringList& query_strings,
                                         const GURL& base_url,
                                         const GURL& url,
                                         const HttpNoVarySearchData& nvs_data) {
  for (auto* node = query_strings.list.head(); node != query_strings.list.end();
       node = node->next()) {
    QueryString* query_string = node->value()->ToQueryString();
    // TODO(crbug.com/382394774): Stop allocating GURLs in a tight loop.
    GURL node_url = query_string->ReconstructOriginalURL(base_url);
    CHECK(node_url.is_valid());
    if (nvs_data.AreEquivalent(url, node_url)) {
      return FindQueryStringResult(query_string, std::move(node_url));
    }
  }
  return std::nullopt;
}

// static
void NoVarySearchCache::ForEachQueryString(
    base::LinkedList<QueryStringListNode>& list,
    base::FunctionRef<void(QueryString*)> f) {
  for (auto* node = list.head(); node != list.end(); node = node->next()) {
    QueryString* query_string = node->value()->ToQueryString();
    f(query_string);
  }
}

// static
void NoVarySearchCache::ForEachQueryString(
    const base::LinkedList<QueryStringListNode>& list,
    base::FunctionRef<void(const QueryString*)> f) {
  for (auto* node = list.head(); node != list.end(); node = node->next()) {
    const QueryString* query_string = node->value()->ToQueryString();
    f(query_string);
  }
}

template <>
struct PickleTraits<NoVarySearchCache::QueryStringList> {
  static void Serialize(
      base::Pickle& pickle,
      const NoVarySearchCache::QueryStringList& query_strings) {
    // base::LinkedList doesn't keep an element count, so we need to count them
    // ourselves.
    size_t size = 0u;
    for (auto* node = query_strings.list.head();
         node != query_strings.list.end(); node = node->next()) {
      ++size;
    }
    WriteToPickle(pickle, base::checked_cast<int>(size));
    NoVarySearchCache::ForEachQueryString(
        query_strings.list,
        [&](const NoVarySearchCache::QueryString* query_string) {
          WriteToPickle(pickle, query_string->query_,
                        query_string->update_time_);
        });
  }

  static std::optional<NoVarySearchCache::QueryStringList> Deserialize(
      base::PickleIterator& iter) {
    NoVarySearchCache::QueryStringList query_string_list;
    size_t size = 0;
    if (!iter.ReadLength(&size)) {
      return std::nullopt;
    }
    for (size_t i = 0; i < size; ++i) {
      // QueryString is not movable or copyable, so it won't work well with
      // PickleTraits. Deserialize it inline instead.
      auto result =
          ReadValuesFromPickle<std::optional<std::string>, base::Time>(iter);
      if (!result) {
        return std::nullopt;
      }
      auto [query, update_time] = std::move(result).value();
      if (query && query->find('#') != std::string_view::npos) {
        // A '#' character must not appear in the query.
        return std::nullopt;
      }
      auto* query_string = new NoVarySearchCache::QueryString(
          std::move(query), query_string_list, update_time);
      // Serialization happens from head to tail, so to deserialize in the same
      // order, we add elements at the tail of the list.
      query_string_list.list.Append(query_string);
    }
    return query_string_list;
  }

  static size_t PickleSize(
      const NoVarySearchCache::QueryStringList& query_strings) {
    size_t estimate = EstimatePickleSize(int{});
    NoVarySearchCache::ForEachQueryString(
        query_strings.list,
        [&](const NoVarySearchCache::QueryString* query_string) {
          estimate += EstimatePickleSize(query_string->query_,
                                         query_string->update_time_);
        });
    return estimate;
  }
};

template <>
struct PickleTraits<NoVarySearchCache::BaseURLCacheKey> {
  static void Serialize(base::Pickle& pickle,
                        const NoVarySearchCache::BaseURLCacheKey& key) {
    WriteToPickle(pickle, *key);
  }

  static std::optional<NoVarySearchCache::BaseURLCacheKey> Deserialize(
      base::PickleIterator& iter) {
    NoVarySearchCache::BaseURLCacheKey key;
    if (!ReadPickleInto(iter, *key)) {
      return std::nullopt;
    }
    return key;
  }

  static size_t PickleSize(const NoVarySearchCache::BaseURLCacheKey& key) {
    return EstimatePickleSize(*key);
  }
};

// static
void PickleTraits<NoVarySearchCache>::Serialize(
    base::Pickle& pickle,
    const NoVarySearchCache& cache) {
  // `size_t` is different sizes on 32-bit and 64-bit platforms. For a
  // consistent format, serialize as int. This will crash if someone creates a
  // NoVarySearchCache which supports over 2 billion entries, which would be a
  // terrible idea anyway.
  int max_size_as_int = base::checked_cast<int>(cache.max_size_);
  int size_as_int = base::checked_cast<int>(cache.size_);

  // `lru_` is reconstructed during deserialization and so doesn't need to be
  // stored explicitly.
  WriteToPickle(pickle, size_as_int, max_size_as_int, cache.map_);
}

// static
std::optional<NoVarySearchCache> PickleTraits<NoVarySearchCache>::Deserialize(
    base::PickleIterator& iter) {
  const std::optional<int> maybe_size = ReadValueFromPickle<int>(iter);
  if (!maybe_size || *maybe_size < 0) {
    return std::nullopt;
  }
  const size_t size = static_cast<size_t>(*maybe_size);
  const std::optional<int> maybe_max_size = ReadValueFromPickle<int>(iter);
  if (!maybe_max_size || *maybe_max_size < 1) {
    return std::nullopt;
  }
  const size_t max_size = static_cast<size_t>(*maybe_max_size);

  if (size > max_size) {
    return std::nullopt;
  }

  NoVarySearchCache cache(max_size);
  cache.size_ = size;
  if (!ReadPickleInto(iter, cache.map_)) {
    return std::nullopt;
  }

  using QueryString = NoVarySearchCache::QueryString;
  // Get a list of every QueryString object in the map so that we can sort
  // them to reconstruct the `lru_` list. std::multimap is used here as a
  // workaround for the excessive binary size cost of std::sort.
  std::multimap<base::Time, QueryString*> all_query_strings;
  for (auto& [base_url_cache_key, data_map] : cache.map_) {
    for (auto& [nvs_data, query_string_list] : data_map) {
      query_string_list.nvs_data_ref = &nvs_data;
      query_string_list.key_ref = &base_url_cache_key;
      NoVarySearchCache::ForEachQueryString(
          query_string_list.list, [&](QueryString* query_string) {
            all_query_strings.emplace(query_string->update_time(),
                                      query_string);
          });
    }
  }
  if (size != all_query_strings.size()) {
    return std::nullopt;
  }

  // Insert each entry at the head of the list, so that the oldest entry ends
  // up at the tail.
  for (auto [_, qs] : all_query_strings) {
    qs->LruNode::InsertBefore(cache.lru_.head());
  }

  return cache;
}

// static
size_t PickleTraits<NoVarySearchCache>::PickleSize(
    const NoVarySearchCache& cache) {
  // `size_` and `max_size_` are pickled as ints.
  return EstimatePickleSize(int{}, int{}, cache.map_);
}

}  // namespace net

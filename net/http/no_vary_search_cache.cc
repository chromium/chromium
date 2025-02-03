// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/no_vary_search_cache.h"

#include <compare>
#include <iostream>
#include <tuple>
#include <type_traits>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/time/time.h"
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
  static void CreateAndInsert(std::optional<std::string_view> query,
                              QueryStringList& query_string_list,
                              base::LinkedList<LruNode>& lru_list) {
    DCHECK(!query || query->find('#') == std::string_view::npos)
        << "Query contained a '#' character, meaning that the URL reassembly "
           "will not work correctly because the '#' will be re-interpreted as "
           "the start of a fragment. This should not happen. Query was '"
        << query.value() << "'";
    // This use of bare new is needed because base::LinkedList does not have
    // ownership semantics.
    auto* query_string = new QueryString(query, query_string_list);
    query_string->LruNode::InsertBefore(lru_list.head());
    query_string->QueryStringListNode::InsertBefore(
        query_string_list.list.head());
  }

  // Not copyable or movable.
  QueryString(const QueryString&) = delete;
  QueryString& operator=(const QueryString&) = delete;

  // Removes this object from both lists and deletes it.
  void RemoveAndDelete() {
    LruNode::RemoveFromList();
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

  QueryStringList& query_string_list_ref() {
    return query_string_list_ref_.get();
  }

  base::Time insertion_time() const { return insertion_time_; }

  void UpdateInsertionTime() { insertion_time_ = base::Time::Now(); }

  // Return the original GURL that this entry was constructed from (not
  // including any fragment). It's important to use this method to correctly
  // reconstruct URLs that have an empty query (end in '?').
  GURL ReconstructOriginalURL(const GURL& base_url) {
    if (!query_.has_value()) {
      return base_url;
    }

    GURL::Replacements replacements;
    replacements.SetQueryStr(query_.value());
    return base_url.ReplaceComponents(replacements);
  }

  EraseHandle CreateEraseHandle() {
    return EraseHandle(weak_factory_.GetWeakPtr());
  }

 private:
  QueryString(std::optional<std::string_view> query,
              QueryStringList& query_string_list)
      : query_(query),
        query_string_list_ref_(query_string_list),
        insertion_time_(base::Time::Now()) {}

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
  // the cache so that it can be erased efficiently.
  const raw_ref<QueryStringList> query_string_list_ref_;

  // `insertion_time_` breaks ties when there are multiple possible matches. The
  // most recent entry will be used as it is most likely to still exist in the
  // disk cache.
  base::Time insertion_time_;

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

NoVarySearchCache::NoVarySearchCache(size_t max_size) : max_size_(max_size) {
  CHECK_GT(max_size_, 0u);
}

NoVarySearchCache::~NoVarySearchCache() {
  map_.clear();
  // Clearing the map should have freed all the QueryString objects.
  CHECK(lru_.empty());
}

std::optional<NoVarySearchCache::LookupResult> NoVarySearchCache::Lookup(
    const HttpRequestInfo& request) {
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
    if (result && (!best_match || best_match->insertion_time() <
                                      result->match->insertion_time())) {
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
  const BaseURLCacheKey cache_key(maybe_cache_key.value());
  const auto [it, _] = map_.try_emplace(cache_key);
  DataMapType& data_map = it->second;
  const auto [data_it, inserted] =
      data_map.emplace(std::move(*maybe_nvs_data), it->first);
  const HttpNoVarySearchData& nvs_data = data_it->first;
  QueryStringList& query_strings = data_it->second;
  if (inserted) {
    query_strings.nvs_data_ref = &nvs_data;
  } else {
    // There was already an entry for this `nvs_data`. We need to check if it
    // has a match for the URL we're trying to insert. If it does, we should
    // update or replace the existing QueryString.
    if (auto result =
            FindQueryStringInList(query_strings, base_url, url, nvs_data)) {
      QueryString* match = result->match;
      if (match->query() == query) {
        // In the exact match case we can use the existing QueryString object.
        match->UpdateInsertionTime();
        match->MoveToHead(query_strings.list);
        match->MoveToHead(lru_);
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
  QueryString::CreateAndInsert(query, query_strings, lru_);
  EvictIfOverfull();
}

void NoVarySearchCache::Erase(EraseHandle handle) {
  if (QueryString* query_string = handle.query_string_.get()) {
    EraseQuery(query_string);
  }
}

// This is out-of-line to discourage inlining so the bots can detect if it is
// accidentally linked into the binary.
size_t NoVarySearchCache::GetSizeForTesting() const {
  return size_;
}

bool NoVarySearchCache::IsTopLevelMapEmptyForTesting() const {
  return map_.empty();
}

NoVarySearchCache::QueryStringList::QueryStringList(const BaseURLCacheKey& key)
    : key_ref(key) {}

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
    const HttpNoVarySearchData* nvs_data_ref = query_strings.nvs_data_ref.get();
    const BaseURLCacheKey& key_ref = query_strings.key_ref.get();
    const auto map_it = map_.find(key_ref);
    CHECK(map_it != map_.end());
    const size_t removed_count = map_it->second.erase(*nvs_data_ref);
    CHECK_EQ(removed_count, 1u);
    if (map_it->second.empty()) {
      map_.erase(map_it);
    }
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

}  // namespace net

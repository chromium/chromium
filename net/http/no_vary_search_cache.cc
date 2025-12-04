// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/no_vary_search_cache.h"

#include <algorithm>
#include <map>
#include <ostream>
#include <tuple>
#include <type_traits>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/map_util.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/pickle.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/types/expected_macros.h"
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
std::string_view ExtractBaseURL(const GURL& url) {
  CHECK(url.is_valid());
  const std::string_view spec = url.spec();
  const size_t query_pos = spec.find('?');
  if (query_pos != std::string_view::npos) {
    return spec.substr(0, query_pos);
  }
  const size_t ref_pos = spec.find('#');
  if (ref_pos != std::string_view::npos) {
    return spec.substr(0, ref_pos);
  }
  return spec;
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
GURL ReconstructOriginalURLFromQuery(std::string_view base_url,
                                     const std::optional<std::string>& query) {
  if (!query.has_value()) {
    return GURL(base_url);
  }

  return GURL(base::StrCat({base_url, "?", query.value()}));
}

}  // namespace

// Query is the leaf entry type for the cache. Its main purpose is to hold the
// query string, ie. everything between the "?" and the "#" in the original URL.
// Combined with the `base_url`, this can be used to reconstruct the original
// URL that was used to store the original request in the disk cache.
class NoVarySearchCache::Query final : public base::LinkNode<Query> {
 public:
  using PassKey = base::PassKey<Query>;

  // Creates a Query and adds it to `lru_list`.
  static std::unique_ptr<Query> CreateAndInsert(
      std::optional<std::string> query,
      base::LinkedList<Query>& lru_list,
      base::Time update_time,
      const std::string* canonicalized_query,
      Queries* queries) {
    DCHECK(!query || query->find('#') == std::string_view::npos)
        << "Query contained a '#' character, meaning that the URL reassembly "
           "will not work correctly because the '#' will be re-interpreted as "
           "the start of a fragment. This should not happen. Query was '"
        << query.value() << "'";
    auto created = std::make_unique<Query>(PassKey(), query, update_time);
    created->canonicalized_query_ = canonicalized_query;
    created->queries_ = queries;
    created->InsertBefore(lru_list.head());
    return created;
  }

  Query(PassKey, std::optional<std::string_view> query, base::Time update_time)
      : query_(query), update_time_(update_time) {}

  // Not copyable or movable.
  Query(const Query&) = delete;
  Query& operator=(const Query&) = delete;

  // Removes this object from the LRU list.
  ~Query() {
    // During deserialization a Query is not inserted into the `lru_` list
    // until the end. If deserialization fails before then, it can be deleted
    // without ever being inserted into the `lru_` list.
    if (next()) {
      CHECK(previous());
      RemoveFromList();
    }
  }

  // Moves this object to the head of `linked_list`.
  void MoveToHead(base::LinkedList<Query>& linked_list) {
    auto* head = linked_list.head();
    if (head != this) {
      MoveBeforeNode(linked_list.head()->value());
    }
  }

  const std::optional<std::string>& query() const { return query_; }

  const std::string& canonicalized_query() const {
    return *canonicalized_query_;
  }

  Queries& queries() const { return *queries_; }

  base::Time update_time() const { return update_time_; }

  void set_update_time(base::Time update_time) { update_time_ = update_time; }

  // Return the original GURL that this entry was constructed from (not
  // including any fragment). It's important to use this method to correctly
  // reconstruct URLs that have an empty query (end in '?').
  GURL ReconstructOriginalURL(const std::string& base_url) const {
    return ReconstructOriginalURLFromQuery(base_url, query_);
  }

  EraseHandle CreateEraseHandle() {
    return EraseHandle(weak_factory_.GetWeakPtr());
  }

  void set_canonicalized_query(const std::string* canonicalized_query) {
    canonicalized_query_ = canonicalized_query;
  }

  void set_queries(Queries* queries) { queries_ = queries; }

 private:
  friend struct PickleTraits<std::unique_ptr<Query>>;

  // Deserialization is implemented here for easy access to the PassKey.
  static std::optional<std::unique_ptr<Query>> Deserialize(
      base::PickleIterator& iter) {
    std::optional<std::string> query;
    base::Time update_time;
    if (!ReadPickleInto(iter, query, update_time)) {
      return std::nullopt;
    }
    // The other fields will be filled in in a second pass while deserializing
    // the top-level NoVarySearchCache object.
    return std::make_unique<Query>(PassKey(), std::move(query), update_time);
  }

  // Moves this object in front of `node`.
  void MoveBeforeNode(Query* node) {
    CHECK_NE(node, this);
    RemoveFromList();
    InsertBefore(node);
  }

  // No-Vary-Search treats "http://www.example.com/" and
  // "http://www.example.com/?" as the same URL, but the disk cache key treats
  // them as different URLs, so we need to be able to distinguish them to
  // correctly reconstruct the original URL. `query_ == std::nullopt` means that
  // there was no `?` in the original URL, and `query_ == ""` means there was.
  const std::optional<std::string> query_;

  // `canonicalized_query_` allows this entry to be located in the query_map so
  // that it can be erased efficiently. The pointed-to std::string is this
  // object's key in the map, so it is always deleted after this object.
  raw_ptr<const std::string> canonicalized_query_ = nullptr;

  // `queries_` provides a pointer to the structure with the other pointers
  // we need to find this entry in the cache. It is owned by the
  // NVSDataToQueriesMap. This object is always deleted before `queries_`
  // because this object is owned by the `query_map` inside Queries.
  raw_ptr<Queries> queries_ = nullptr;

  // `update_time_` breaks ties when there are multiple possible matches. The
  // most recent entry will be used as it is most likely to still exist in the
  // disk cache.
  base::Time update_time_;

  // EraseHandle uses weak pointers to Query objects to enable an entry to
  // be deleted from the cache if it is found not to be readable from the disk
  // cache.
  base::WeakPtrFactory<Query> weak_factory_{this};
};

NoVarySearchCache::EraseHandle::EraseHandle(base::WeakPtr<Query> query)
    : query_(std::move(query)) {}

NoVarySearchCache::EraseHandle::~EraseHandle() = default;

NoVarySearchCache::EraseHandle::EraseHandle(EraseHandle&& rhs) = default;
NoVarySearchCache::EraseHandle& NoVarySearchCache::EraseHandle::operator=(
    EraseHandle&& rhs) = default;

bool NoVarySearchCache::EraseHandle::EqualsForTesting(
    const EraseHandle& rhs) const {
  return query_.get() == rhs.query_.get();
}

bool NoVarySearchCache::EraseHandle::IsGoneForTesting() const {
  return !query_;
}

NoVarySearchCache::Journal::~Journal() = default;

NoVarySearchCache::Queries::Queries() = default;
NoVarySearchCache::Queries::~Queries() = default;

NoVarySearchCache::Queries::Queries(Queries&&) = default;

NoVarySearchCache::NoVarySearchCache(size_t max_size) : max_size_(max_size) {
  CHECK_GE(max_size_, 1u);
  // We can't serialize if `max_size` won't fit in an int.
  CHECK(base::IsValueInRangeForNumericType<int>(max_size));
}

NoVarySearchCache::NoVarySearchCache(NoVarySearchCache&& rhs)
    : partitions_(std::move(rhs.partitions_)),
      lru_(std::move(rhs.lru_)),
      size_(std::exchange(rhs.size_, 0u)),
      max_size_(rhs.max_size_) {}

NoVarySearchCache::~NoVarySearchCache() {
  partitions_.clear();
  // Clearing the map should have freed all the Query objects.
  CHECK(lru_.empty());
}

std::optional<NoVarySearchCache::LookupResult> NoVarySearchCache::Lookup(
    const HttpRequestInfo& request) {
  bool unused = true;
  return Lookup(request, /*out_base_url_matched=*/unused);
}

std::optional<NoVarySearchCache::LookupResult> NoVarySearchCache::Lookup(
    const HttpRequestInfo& request,
    bool& out_base_url_matched) {
  out_base_url_matched = false;
  const GURL& url = request.url;
  if (!URLIsAcceptable(url)) {
    return std::nullopt;
  }

  TRACE_EVENT("net", "NoVarySearchCache::Lookup");

  // TODO(https://crbug.com/388956603): This does a lot of allocations and
  // string copies. Try to reduce the amount of work done for a miss.
  ASSIGN_OR_RETURN(
      const std::string cache_partition_key,
      HttpCache::GenerateCachePartitionKeyForRequest(request),
      []() -> std::optional<NoVarySearchCache::LookupResult> { return {}; });

  const auto partition_it = partitions_.find(cache_partition_key);
  if (partition_it == partitions_.end()) {
    return std::nullopt;
  }

  auto& [cache_partition_key_ref, base_url_map] = *partition_it;

  const std::string_view base_url_view = ExtractBaseURL(url);
  const auto base_url_map_it = base_url_map.find(base_url_view);
  if (base_url_map_it == base_url_map.end()) {
    return std::nullopt;
  }
  out_base_url_matched = true;

  auto& [base_url_ref, nvs_data_to_queries_map] = *base_url_map_it;
  Query* best_match = nullptr;

  // `nvs_data_to_queries_map` should ideally only have one entry, but if the
  // site has sent different No-Vary-Search header values for the same base URL
  // we need to check them all.
  for (auto& [nvs_data, queries] : nvs_data_to_queries_map) {
    Query* result = FindQuery(queries.query_map, url, nvs_data);
    if (result &&
        (!best_match || best_match->update_time() < result->update_time())) {
      best_match = result;
    }
  }
  if (!best_match) {
    return std::nullopt;
  }

  // This is a hit. Move to head of `lru_` list.
  best_match->MoveToHead(lru_);

  return LookupResult(best_match->ReconstructOriginalURL(base_url_ref),
                      best_match->CreateEraseHandle());
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
  const std::string_view base_url = ExtractBaseURL(url);

  std::optional<std::string> query =
      url.has_query() ? std::make_optional(url.GetQuery()) : std::nullopt;

  ASSIGN_OR_RETURN(const std::string cache_partition_key,
                   HttpCache::GenerateCachePartitionKeyForRequest(request),
                   [] { return; });

  const base::Time update_time = base::Time::Now();

  DoInsert(url, std::move(cache_partition_key), std::string(base_url),
           std::move(maybe_nvs_data.value()), std::move(query), update_time,
           journal_);
}

bool NoVarySearchCache::ClearData(UrlFilterType filter_type,
                                  const base::flat_set<url::Origin>& origins,
                                  const base::flat_set<std::string>& domains,
                                  base::Time delete_begin,
                                  base::Time delete_end) {
  // For simplicity, first collect a list of matching Query objects to erase and
  // then erase them.
  // TODO(https://crbug.com/382394774): Make this algorithm more efficient.
  std::vector<Query*> pending_erase;
  for (auto& [cache_partition_key, base_url_map] : partitions_) {
    for (auto& [base_url_ref, nvs_data_to_queries_map] : base_url_map) {
      const GURL base_url(base_url_ref);
      CHECK(base_url.is_valid());
      // DoesUrlMatchFilter() only looks at the origin of the URL, which is why
      // we don't need to worry about reconstructing the full URL with query.
      if (DoesUrlMatchFilter(filter_type, origins, domains, base_url)) {
        FindQuerysInTimeRange(nvs_data_to_queries_map, delete_begin, delete_end,
                              pending_erase);
      }
    }
  }
  for (Query* query : pending_erase) {
    EraseQuery(query);
  }
  return !pending_erase.empty();
}

void NoVarySearchCache::Erase(EraseHandle handle) {
  if (Query* query = handle.query_.get()) {
    if (journal_) {
      const Queries& queries = query->queries();
      journal_->OnErase(*queries.cache_partition_key_ptr, *queries.base_url_ptr,
                        *queries.nvs_data_ptr, query->query());
    }

    EraseQuery(query);
  }
}

void NoVarySearchCache::SetJournal(Journal* journal) {
  journal_ = journal;
}

void NoVarySearchCache::ReplayInsert(std::string partition_key,
                                     std::string base_url,
                                     HttpNoVarySearchData nvs_data,
                                     std::optional<std::string> query,
                                     base::Time update_time) {
  const GURL base_gurl(base_url);
  if (!BaseURLIsAcceptable(base_gurl)) {
    return;
  }
  // The URL should have been stored in its canonical form.
  if (base_url != base_gurl.spec()) {
    return;
  }

  if (query && query->find('#') != std::string::npos) {
    return;
  }

  // To be extra careful to avoid re-entrancy, explicitly set `journal` to
  // nullptr so that no notification is fired for this insertion.
  ReconstructURLAndDoInsert(std::move(partition_key), std::move(base_url),
                            std::move(nvs_data), std::move(query), update_time,
                            /*journal=*/nullptr);
}

void NoVarySearchCache::ReplayErase(const std::string& partition_key,
                                    const std::string& base_url,
                                    const HttpNoVarySearchData& nvs_data,
                                    const std::optional<std::string>& query) {
  const auto map_it = partitions_.find(partition_key);
  if (map_it == partitions_.end()) {
    return;
  }

  BaseUrlToNVSDataMap& base_url_map = map_it->second;

  const auto base_url_it = base_url_map.find(base_url);
  if (base_url_it == base_url_map.end()) {
    return;
  }

  NVSDataToQueriesMap& nvs_data_to_queries_map = base_url_it->second;

  const auto data_it = nvs_data_to_queries_map.find(nvs_data);
  if (data_it == nvs_data_to_queries_map.end()) {
    return;
  }

  Queries& queries = data_it->second;
  const GURL original_url = ReconstructOriginalURLFromQuery(base_url, query);
  // Since `base_url` was already in the map, it must have been valid. There is
  // no value of `query` that GURL will not canonicalize. So the resulting URL
  // is always valid (even if the query itself was corrupted).
  CHECK(original_url.is_valid());

  const std::string canonicalized_query =
      nvs_data.CanonicalizeQuery(original_url);
  const auto query_it = queries.query_map.find(canonicalized_query);
  if (query_it == queries.query_map.end()) {
    return;
  }

  Query* query_ptr = query_it->second.get();

  // TODO(https://crbug.com/382394774): This could be made more efficient in the
  // case when the map keys need to be deleted since we have `map_it` and
  // `data_it` already available.
  EraseQuery(query_ptr);
}

void NoVarySearchCache::MergeFrom(const NoVarySearchCache& newer) {
  // We cannot use ForEachQuery() here as we need to iterate through the
  // `lru_` linked list in reverse order.
  const auto& newer_lru = newer.lru_;
  for (auto* node = newer_lru.tail(); node != newer_lru.end();
       node = node->previous()) {
    Query* query = node->value();
    const Queries& queries = query->queries();
    const std::string& base_url = *queries.base_url_ptr;
    std::optional<std::string> query_string = query->query();
    CHECK(!query_string || query_string->find('#') == std::string::npos);

    // Pass `journal_` so the merged entries are journalled as insertions.
    ReconstructURLAndDoInsert(*queries.cache_partition_key_ptr, base_url,
                              *queries.nvs_data_ptr, std::move(query_string),
                              query->update_time(), journal_);
  }
}

void NoVarySearchCache::SetMaxSize(size_t max_size) {
  if (max_size == max_size_) {
    return;
  }
  CHECK_GE(max_size, 1u);
  // Evict entries while size_ > max_size_.
  max_size_ = max_size;
  while (size_ > max_size_) {
    EraseQuery(lru_.tail()->value());
  }
}

bool NoVarySearchCache::IsTopLevelMapEmptyForTesting() const {
  return partitions_.empty();
}

void NoVarySearchCache::EvictIfOverfull() {
  CHECK_LE(size_, max_size_ + 1);
  if (size_ == max_size_ + 1) {
    // This happens when an entry is added when the cache is already full.
    // Remove an entry to make `size_` == `max_size_` again.
    EraseQuery(lru_.tail()->value());
  }
}

void NoVarySearchCache::EraseQuery(Query* query) {
  CHECK_GT(size_, 0u);
  --size_;
  Queries& queries = query->queries();
  const std::string& canonicalized_query = query->canonicalized_query();
  const auto query_map_it = queries.query_map.find(canonicalized_query);
  CHECK(query_map_it != queries.query_map.end());
  queries.query_map.erase(query_map_it);
  if (!queries.query_map.empty()) {
    return;
  }

  // The Queries object is now empty, so we should delete it from its parent
  // map. First we have to find its parent map.
  const HttpNoVarySearchData& nvs_data = *queries.nvs_data_ptr;
  const std::string& base_url = *queries.base_url_ptr;
  const std::string& partition_key = *queries.cache_partition_key_ptr;

  const auto partition_it = partitions_.find(partition_key);
  CHECK(partition_it != partitions_.end());
  auto& base_url_map = partition_it->second;
  const auto base_url_it = base_url_map.find(base_url);
  CHECK(base_url_it != base_url_map.end());
  NVSDataToQueriesMap& nvs_data_to_queries_map = base_url_it->second;
  const auto data_it = nvs_data_to_queries_map.find(nvs_data);
  CHECK(data_it != nvs_data_to_queries_map.end());

  nvs_data_to_queries_map.erase(data_it);
  if (!nvs_data_to_queries_map.empty()) {
    return;
  }

  base_url_map.erase(base_url_it);
  if (!base_url_map.empty()) {
    return;
  }

  partitions_.erase(partition_it);
}

void NoVarySearchCache::DoInsert(const GURL& url,
                                 std::string partition_key,
                                 std::string base_url,
                                 HttpNoVarySearchData nvs_data,
                                 std::optional<std::string> query,
                                 base::Time update_time,
                                 Journal* journal) {
  auto [partition_it, partition_inserted] =
      partitions_.try_emplace(std::move(partition_key), BaseUrlToNVSDataMap());
  auto& [cache_partition_key_ref, base_url_map] = *partition_it;
  CHECK(partition_inserted || !base_url_map.empty());

  auto [base_url_it, base_url_inserted] =
      base_url_map.try_emplace(std::move(base_url), NVSDataToQueriesMap());
  auto& [base_url_ref, nvs_data_to_queries_map] = *base_url_it;
  CHECK(base_url_inserted || !nvs_data_to_queries_map.empty());

  auto [nvs_data_it, nvs_data_inserted] =
      nvs_data_to_queries_map.try_emplace(std::move(nvs_data), Queries());
  auto& [nvs_data_ref, queries] = *nvs_data_it;
  if (nvs_data_inserted) {
    queries.nvs_data_ptr = &nvs_data_ref;
    queries.base_url_ptr = &base_url_ref;
    queries.cache_partition_key_ptr = &cache_partition_key_ref;
  } else {
    CHECK(!queries.query_map.empty());
    CHECK_EQ(queries.nvs_data_ptr, &nvs_data_ref);
    CHECK_EQ(queries.base_url_ptr, &base_url_ref);
    CHECK_EQ(queries.cache_partition_key_ptr, &cache_partition_key_ref);
  }

  auto call_journal = [&journal, &cache_partition_key_ref, &base_url_ref,
                       &nvs_data_ref,
                       update_time](const std::optional<std::string>& query) {
    if (journal) {
      journal->OnInsert(cache_partition_key_ref, base_url_ref, nvs_data_ref,
                        query, update_time);
    }
  };
  const std::string canonicalized_query = nvs_data_ref.CanonicalizeQuery(url);
  auto [query_it, query_inserted] =
      queries.query_map.try_emplace(canonicalized_query, nullptr);
  if (!query_inserted) {
    // There was already an entry for this `canonicalized_query`. We need
    // to check if it is an exact match for the URL we're trying to insert. If
    // it is, we don't need to replace it.
    auto& query_ptr = query_it->second;
    if (query_ptr->query() == query) {
      // It's an exact match. Just mark as freshly updated and used.
      query_ptr->set_update_time(update_time);
      query_ptr->MoveToHead(lru_);

      call_journal(query_ptr->query());

      return;
    }

    // Otherwise, replace it with a newly-created Query.
    query_ptr = nullptr;
    CHECK_GT(size_, 0u);
    --size_;
  }
  auto& [canonicalized_query_ref, query_ptr] = *query_it;
  query_ptr = Query::CreateAndInsert(std::move(query), lru_, update_time,
                                     &canonicalized_query_ref, &queries);
  CHECK_LE(size_, max_size_);
  ++size_;
  call_journal(query_ptr->query());
  EvictIfOverfull();
}

void NoVarySearchCache::ReconstructURLAndDoInsert(
    std::string partition_key,
    std::string base_url,
    HttpNoVarySearchData nvs_data,
    std::optional<std::string> query,
    base::Time update_time,
    Journal* journal) {
  const GURL url = ReconstructOriginalURLFromQuery(base_url, query);
  DoInsert(url, std::move(partition_key), std::move(base_url),
           std::move(nvs_data), std::move(query), update_time, journal);
}

// static
void NoVarySearchCache::FindQuerysInTimeRange(
    NVSDataToQueriesMap& nvs_data_to_queries_map,
    base::Time delete_begin,
    base::Time delete_end,
    std::vector<Query*>& matches) {
  for (auto& [_, queries] : nvs_data_to_queries_map) {
    ForEachQuery(queries, [&](Query* query) {
      const base::Time update_time = query->update_time();
      if ((delete_begin.is_null() || delete_begin <= update_time) &&
          (delete_end.is_max() || delete_end > update_time)) {
        matches.push_back(query);
      }
    });
  }
}

// static
NoVarySearchCache::Query* NoVarySearchCache::FindQuery(
    CanonicalizedQueryToQueryMap& query_map,
    const GURL& url,
    const HttpNoVarySearchData& nvs_data) {
  const std::string canonicalized_query = nvs_data.CanonicalizeQuery(url);
  return base::FindPtrOrNull(query_map, std::move(canonicalized_query));
}

// static
void NoVarySearchCache::ForEachQuery(Queries& queries,
                                     base::FunctionRef<void(Query*)> f) {
  for (auto& [_, query_ptr] : queries.query_map) {
    f(query_ptr.get());
  }
}

template <>
struct PickleTraits<std::unique_ptr<NoVarySearchCache::Query>> {
  using Query = NoVarySearchCache::Query;

  static void Serialize(base::Pickle& pickle,
                        const std::unique_ptr<Query>& query) {
    WriteToPickle(pickle, query->query(), query->update_time());
  }

  static std::optional<std::unique_ptr<Query>> Deserialize(
      base::PickleIterator& iter) {
    return Query::Deserialize(iter);
  }

  static size_t PickleSize(const std::unique_ptr<Query>& query) {
    return EstimatePickleSize(query->query(), query->update_time());
  }
};

template <>
struct PickleTraits<NoVarySearchCache::Queries> {
  static void Serialize(base::Pickle& pickle,
                        const NoVarySearchCache::Queries& queries) {
    WriteToPickle(pickle, queries.query_map);
  }

  static std::optional<NoVarySearchCache::Queries> Deserialize(
      base::PickleIterator& iter) {
    NoVarySearchCache::Queries queries;
    if (!ReadPickleInto(iter, queries.query_map)) {
      return std::nullopt;
    }
    return queries;
  }

  static size_t PickleSize(const NoVarySearchCache::Queries& queries) {
    return EstimatePickleSize(queries.query_map);
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
  WriteToPickle(pickle, size_as_int, max_size_as_int, cache.partitions_);
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
  if (!ReadPickleInto(iter, cache.partitions_)) {
    return std::nullopt;
  }

  using Query = NoVarySearchCache::Query;
  // Get a list of every Query object in the map so that we can sort
  // them to reconstruct the `lru_` list. std::multimap is used here as a
  // workaround for the excessive binary size cost of std::sort.
  std::multimap<base::Time, Query*> all_queries;
  for (auto& [cache_partition_key, base_url_map] : cache.partitions_) {
    for (auto& [base_url_ref, nvs_data_to_queries_map] : base_url_map) {
      for (auto& [nvs_data, queries] : nvs_data_to_queries_map) {
        queries.nvs_data_ptr = &nvs_data;
        queries.base_url_ptr = &base_url_ref;
        queries.cache_partition_key_ptr = &cache_partition_key;
        for (auto& [canonicalized_query, query_ptr] : queries.query_map) {
          query_ptr->set_canonicalized_query(&canonicalized_query);
          query_ptr->set_queries(&queries);
          all_queries.emplace(query_ptr->update_time(), query_ptr.get());
        }
      }
    }
  }
  if (size != all_queries.size()) {
    return std::nullopt;
  }

  // Insert each entry at the head of the list, so that the oldest entry ends
  // up at the tail.
  for (auto [_, qs] : all_queries) {
    qs->InsertBefore(cache.lru_.head());
  }

  return cache;
}

// static
size_t PickleTraits<NoVarySearchCache>::PickleSize(
    const NoVarySearchCache& cache) {
  // `size_` and `max_size_` are pickled as ints.
  return EstimatePickleSize(int{}, int{}, cache.partitions_);
}

}  // namespace net

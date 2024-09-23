// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/host_resolver_cache.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "net/base/network_anonymization_key.h"
#include "net/dns/host_resolver_internal_result.h"
#include "net/dns/public/dns_query_type.h"
#include "net/dns/public/host_resolver_source.h"
#include "url/third_party/mozilla/url_parse.h"
#include "url/url_canon.h"
#include "url/url_canon_stdstring.h"

namespace net {

namespace {

constexpr std::string_view kNakKey = "network_anonymization_key";
constexpr std::string_view kSourceKey = "source";
constexpr std::string_view kSecureKey = "secure";
constexpr std::string_view kResultKey = "result";
constexpr std::string_view kStalenessGenerationKey = "staleness_generation";
constexpr std::string_view kMaxEntriesKey = "max_entries";
constexpr std::string_view kEntriesKey = "entries";

}  // namespace

HostResolverCache::Key::~Key() = default;

HostResolverCache::StaleLookupResult::StaleLookupResult(
    const HostResolverInternalResult& result,
    std::optional<base::TimeDelta> expired_by,
    bool stale_by_generation)
    : result(result),
      expired_by(expired_by),
      stale_by_generation(stale_by_generation) {}

HostResolverCache::HostResolverCache(size_t max_results,
                                     const base::Clock& clock,
                                     const base::TickClock& tick_clock)
    : max_entries_(max_results), clock_(clock), tick_clock_(tick_clock) {
  DCHECK_GT(max_entries_, 0u);
}

HostResolverCache::~HostResolverCache() = default;

HostResolverCache::HostResolverCache(HostResolverCache&&) = default;

HostResolverCache& HostResolverCache::operator=(HostResolverCache&&) = default;

const HostResolverInternalResult* HostResolverCache::Lookup(
    std::string_view domain_name,
    const NetworkAnonymizationKey& network_anonymization_key,
    DnsQueryType query_type,
    HostResolverSource source,
    std::optional<bool> secure) const {
  std::vector<EntryMap::const_iterator> candidates = LookupInternal(
      domain_name, network_anonymization_key, query_type, source, secure);

  // Get the most secure, last-matching (which is first in the vector returned
  // by LookupInternal()) non-expired result.
  base::TimeTicks now_ticks = tick_clock_->NowTicks();
  base::Time now = clock_->Now();
  HostResolverInternalResult* most_secure_result = nullptr;
  for (const EntryMap::const_iterator& candidate : candidates) {
    DCHECK(candidate->second.result->timed_expiration().has_value());

    if (candidate->second.IsStale(now, now_ticks, staleness_generation_)) {
      continue;
    }

    // If the candidate is secure, or all results are insecure, no need to check
    // any more.
    if (candidate->second.secure || !secure.value_or(true)) {
      return candidate->second.result.get();
    } else if (most_secure_result == nullptr) {
      most_secure_result = candidate->second.result.get();
    }
  }

  return most_secure_result;
}

std::optional<HostResolverCache::StaleLookupResult>
HostResolverCache::LookupStale(
    std::string_view domain_name,
    const NetworkAnonymizationKey& network_anonymization_key,
    DnsQueryType query_type,
    HostResolverSource source,
    std::optional<bool> secure) const {
  std::vector<EntryMap::const_iterator> candidates = LookupInternal(
      domain_name, network_anonymization_key, query_type, source, secure);

  // Get the least expired, most secure result.
  base::TimeTicks now_ticks = tick_clock_->NowTicks();
  base::Time now = clock_->Now();
  const Entry* best_match = nullptr;
  base::TimeDelta best_match_time_until_expiration;
  for (const EntryMap::const_iterator& candidate : candidates) {
    DCHECK(candidate->second.result->timed_expiration().has_value());

    base::TimeDelta candidate_time_until_expiration =
        candidate->second.TimeUntilExpiration(now, now_ticks);

    if (!candidate->second.IsStale(now, now_ticks, staleness_generation_) &&
        (candidate->second.secure || !secure.value_or(true))) {
      // If a non-stale candidate is secure, or all results are insecure, no
      // need to check any more.
      best_match = &candidate->second;
      best_match_time_until_expiration = candidate_time_until_expiration;
      break;
    } else if (best_match == nullptr ||
               (!candidate->second.IsStale(now, now_ticks,
                                           staleness_generation_) &&
                best_match->IsStale(now, now_ticks, staleness_generation_)) ||
               candidate->second.staleness_generation >
                   best_match->staleness_generation ||
               (candidate->second.staleness_generation ==
                    best_match->staleness_generation &&
                candidate_time_until_expiration >
                    best_match_time_until_expiration) ||
               (candidate->second.staleness_generation ==
                    best_match->staleness_generation &&
                candidate_time_until_expiration ==
                    best_match_time_until_expiration &&
                candidate->second.secure && !best_match->secure)) {
      best_match = &candidate->second;
      best_match_time_until_expiration = candidate_time_until_expiration;
    }
  }

  if (best_match == nullptr) {
    return std::nullopt;
  } else {
    std::optional<base::TimeDelta> expired_by;
    if (best_match_time_until_expiration.is_negative()) {
      expired_by = best_match_time_until_expiration.magnitude();
    }
    return StaleLookupResult(
        *best_match->result, expired_by,
        best_match->staleness_generation != staleness_generation_);
  }
}

void HostResolverCache::Set(
    std::unique_ptr<HostResolverInternalResult> result,
    const NetworkAnonymizationKey& network_anonymization_key,
    HostResolverSource source,
    bool secure) {
  Set(std::move(result), network_anonymization_key, source, secure,
      /*replace_existing=*/true, staleness_generation_);
}

void HostResolverCache::MakeAllResultsStale() {
  ++staleness_generation_;
}

base::Value HostResolverCache::Serialize() const {
  // Do not serialize any entries without a persistable anonymization key
  // because it is required to store and restore entries with the correct
  // annonymization key. A non-persistable anonymization key is typically used
  // for short-lived contexts, and associated entries are not expected to be
  // useful after persistence to disk anyway.
  return SerializeEntries(/*serialize_staleness_generation=*/false,
                          /*require_persistable_anonymization_key=*/true);
}

bool HostResolverCache::RestoreFromValue(const base::Value& value) {
  const base::Value::List* list = value.GetIfList();
  if (!list) {
    return false;
  }

  for (const base::Value& list_value : *list) {
    // Simply stop on reaching max size rather than attempting to figure out if
    // any current entries should be evicted over the deserialized entries.
    if (entries_.size() == max_entries_) {
      return true;
    }

    const base::Value::Dict* dict = list_value.GetIfDict();
    if (!dict) {
      return false;
    }

    const base::Value* anonymization_key_value = dict->Find(kNakKey);
    NetworkAnonymizationKey anonymization_key;
    if (!anonymization_key_value ||
        !NetworkAnonymizationKey::FromValue(*anonymization_key_value,
                                            &anonymization_key)) {
      return false;
    }

    const base::Value* source_value = dict->Find(kSourceKey);
    std::optional<HostResolverSource> source =
        source_value == nullptr ? std::nullopt
                                : HostResolverSourceFromValue(*source_value);
    if (!source.has_value()) {
      return false;
    }

    std::optional<bool> secure = dict->FindBool(kSecureKey);
    if (!secure.has_value()) {
      return false;
    }

    const base::Value* result_value = dict->Find(kResultKey);
    std::unique_ptr<HostResolverInternalResult> result =
        result_value == nullptr
            ? nullptr
            : HostResolverInternalResult::FromValue(*result_value);
    if (!result || !result->timed_expiration().has_value()) {
      return false;
    }

    // `staleness_generation_ - 1` to make entry stale-by-generation.
    Set(std::move(result), anonymization_key, source.value(), secure.value(),
        /*replace_existing=*/false, staleness_generation_ - 1);
  }

  CHECK_LE(entries_.size(), max_entries_);
  return true;
}

base::Value HostResolverCache::SerializeForLogging() const {
  base::Value::Dict dict;

  dict.Set(kMaxEntriesKey, base::checked_cast<int>(max_entries_));
  dict.Set(kStalenessGenerationKey, staleness_generation_);

  // Include entries with non-persistable anonymization keys, so the log can
  // contain all entries. Restoring from this serialization is not supported.
  dict.Set(kEntriesKey,
           SerializeEntries(/*serialize_staleness_generation=*/true,
                            /*require_persistable_anonymization_key=*/false));

  return base::Value(std::move(dict));
}

HostResolverCache::Entry::Entry(
    std::unique_ptr<HostResolverInternalResult> result,
    HostResolverSource source,
    bool secure,
    int staleness_generation)
    : result(std::move(result)),
      source(source),
      secure(secure),
      staleness_generation(staleness_generation) {}

HostResolverCache::Entry::~Entry() = default;

HostResolverCache::Entry::Entry(Entry&&) = default;

HostResolverCache::Entry& HostResolverCache::Entry::operator=(Entry&&) =
    default;

bool HostResolverCache::Entry::IsStale(base::Time now,
                                       base::TimeTicks now_ticks,
                                       int current_staleness_generation) const {
  return staleness_generation != current_staleness_generation ||
         TimeUntilExpiration(now, now_ticks).is_negative();
}

base::TimeDelta HostResolverCache::Entry::TimeUntilExpiration(
    base::Time now,
    base::TimeTicks now_ticks) const {
  if (result->expiration().has_value()) {
    return result->expiration().value() - now_ticks;
  } else {
    DCHECK(result->timed_expiration().has_value());
    return result->timed_expiration().value() - now;
  }
}

std::vector<HostResolverCache::EntryMap::const_iterator>
HostResolverCache::LookupInternal(
    std::string_view domain_name,
    const NetworkAnonymizationKey& network_anonymization_key,
    DnsQueryType query_type,
    HostResolverSource source,
    std::optional<bool> secure) const {
  auto matches = std::vector<EntryMap::const_iterator>();

  if (entries_.empty()) {
    return matches;
  }

  std::string canonicalized;
  url::StdStringCanonOutput output(&canonicalized);
  url::CanonHostInfo host_info;

  url::CanonicalizeHostVerbose(domain_name.data(),
                               url::Component(0, domain_name.size()), &output,
                               &host_info);

  // For performance, when canonicalization can't canonicalize, minimize string
  // copies and just reuse the input std::string_view. This optimization
  // prevents easily reusing a MaybeCanoncalize util with similar code.
  std::string_view lookup_name = domain_name;
  if (host_info.family == url::CanonHostInfo::Family::NEUTRAL) {
    output.Complete();
    lookup_name = canonicalized;
  }

  auto range = entries_.equal_range(
      KeyRef{lookup_name, raw_ref(network_anonymization_key)});
  if (range.first == entries_.cend() || range.second == entries_.cbegin() ||
      range.first == range.second) {
    return matches;
  }

  // Iterate in reverse order to return most-recently-added entry first.
  auto it = --range.second;
  while (true) {
    if ((query_type == DnsQueryType::UNSPECIFIED ||
         it->second.result->query_type() == DnsQueryType::UNSPECIFIED ||
         query_type == it->second.result->query_type()) &&
        (source == HostResolverSource::ANY || source == it->second.source) &&
        (!secure.has_value() || secure.value() == it->second.secure)) {
      matches.push_back(it);
    }

    if (it == range.first) {
      break;
    }
    --it;
  }

  return matches;
}

void HostResolverCache::Set(
    std::unique_ptr<HostResolverInternalResult> result,
    const NetworkAnonymizationKey& network_anonymization_key,
    HostResolverSource source,
    bool secure,
    bool replace_existing,
    int staleness_generation) {
  DCHECK(result);
  // Result must have at least a timed expiration to be a cacheable result.
  DCHECK(result->timed_expiration().has_value());

  std::vector<EntryMap::const_iterator> matches =
      LookupInternal(result->domain_name(), network_anonymization_key,
                     result->query_type(), source, secure);

  if (!matches.empty() && !replace_existing) {
    // Matches already present that are not to be replaced.
    return;
  }

  for (const EntryMap::const_iterator& match : matches) {
    entries_.erase(match);
  }

  std::string domain_name = result->domain_name();
  entries_.emplace(
      Key(std::move(domain_name), network_anonymization_key),
      Entry(std::move(result), source, secure, staleness_generation));

  if (entries_.size() > max_entries_) {
    EvictEntries();
  }
}

// Remove all stale entries, or if none stale, the soonest-to-expire,
// least-secure entry.
void HostResolverCache::EvictEntries() {
  base::TimeTicks now_ticks = tick_clock_->NowTicks();
  base::Time now = clock_->Now();

  bool stale_found = false;
  base::TimeDelta soonest_time_till_expriation = base::TimeDelta::Max();
  std::optional<EntryMap::const_iterator> best_for_removal;

  auto it = entries_.cbegin();
  while (it != entries_.cend()) {
    if (it->second.IsStale(now, now_ticks, staleness_generation_)) {
      stale_found = true;
      it = entries_.erase(it);
    } else {
      base::TimeDelta time_till_expiration =
          it->second.TimeUntilExpiration(now, now_ticks);

      if (!best_for_removal.has_value() ||
          time_till_expiration < soonest_time_till_expriation ||
          (time_till_expiration == soonest_time_till_expriation &&
           best_for_removal.value()->second.secure && !it->second.secure)) {
        soonest_time_till_expriation = time_till_expiration;
        best_for_removal = it;
      }

      ++it;
    }
  }

  if (!stale_found) {
    CHECK(best_for_removal.has_value());
    entries_.erase(best_for_removal.value());
  }

  CHECK_LE(entries_.size(), max_entries_);
}

base::Value HostResolverCache::SerializeEntries(
    bool serialize_staleness_generation,
    bool require_persistable_anonymization_key) const {
  base::Value::List list;

  for (const auto& [key, entry] : entries_) {
    base::Value::Dict dict;

    if (serialize_staleness_generation) {
      dict.Set(kStalenessGenerationKey, entry.staleness_generation);
    }

    base::Value anonymization_key_value;
    if (!key.network_anonymization_key.ToValue(&anonymization_key_value)) {
      if (require_persistable_anonymization_key) {
        continue;
      } else {
        // If the caller doesn't care about anonymization keys that can be
        // serialized and restored, construct a serialization just for the sake
        // of logging information.
        anonymization_key_value =
            base::Value("Non-persistable network anonymization key: " +
                        key.network_anonymization_key.ToDebugString());
      }
    }

    dict.Set(kNakKey, std::move(anonymization_key_value));
    dict.Set(kSourceKey, ToValue(entry.source));
    dict.Set(kSecureKey, entry.secure);
    dict.Set(kResultKey, entry.result->ToValue());

    list.Append(std::move(dict));
  }

  return base::Value(std::move(list));
}

}  // namespace net

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/host_resolver_cache.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/strings/string_piece.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "net/base/network_anonymization_key.h"
#include "net/dns/host_resolver_internal_result.h"
#include "net/dns/public/dns_query_type.h"
#include "net/dns/public/host_resolver_source.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/third_party/mozilla/url_parse.h"
#include "url/url_canon.h"
#include "url/url_canon_stdstring.h"

namespace net {

HostResolverCache::StaleLookupResult::StaleLookupResult(
    const HostResolverInternalResult& result,
    absl::optional<base::TimeDelta> expired_by,
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
    base::StringPiece domain_name,
    const NetworkAnonymizationKey& network_anonymization_key,
    DnsQueryType query_type,
    HostResolverSource source,
    absl::optional<bool> secure) const {
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

absl::optional<HostResolverCache::StaleLookupResult>
HostResolverCache::LookupStale(
    base::StringPiece domain_name,
    const NetworkAnonymizationKey& network_anonymization_key,
    DnsQueryType query_type,
    HostResolverSource source,
    absl::optional<bool> secure) const {
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
    return absl::nullopt;
  } else {
    absl::optional<base::TimeDelta> expired_by;
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
  DCHECK(result);
  // Result must have at least a timed expiration to be a cacheable result.
  DCHECK(result->timed_expiration().has_value());

  std::vector<EntryMap::const_iterator> matches =
      LookupInternal(result->domain_name(), network_anonymization_key,
                     result->query_type(), source, secure);

  for (const EntryMap::const_iterator& match : matches) {
    entries_.erase(match);
  }

  std::string domain_name = result->domain_name();
  entries_.emplace(
      Key(std::move(domain_name), network_anonymization_key),
      Entry(std::move(result), source, secure, staleness_generation_));

  if (entries_.size() > max_entries_) {
    EvictEntries();
  }
}

void HostResolverCache::MakeAllResultsStale() {
  ++staleness_generation_;
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
    base::StringPiece domain_name,
    const NetworkAnonymizationKey& network_anonymization_key,
    DnsQueryType query_type,
    HostResolverSource source,
    absl::optional<bool> secure) const {
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
  // copies and just reuse the input StringPiece. This optimization prevents
  // easily reusing a MaybeCanoncalize util with similar code.
  base::StringPiece lookup_name = domain_name;
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

// Remove all stale entries, or if none stale, the soonest-to-expire,
// least-secure entry.
void HostResolverCache::EvictEntries() {
  base::TimeTicks now_ticks = tick_clock_->NowTicks();
  base::Time now = clock_->Now();

  bool stale_found = false;
  base::TimeDelta soonest_time_till_expriation = base::TimeDelta::Max();
  absl::optional<EntryMap::const_iterator> best_for_removal;

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

}  // namespace net

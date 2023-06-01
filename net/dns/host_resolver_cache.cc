// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/host_resolver_cache.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/string_piece.h"
#include "base/time/clock.h"
#include "net/base/network_anonymization_key.h"
#include "net/dns/host_resolver_internal_result.h"
#include "net/dns/public/dns_query_type.h"
#include "net/dns/public/host_resolver_source.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/third_party/mozilla/url_parse.h"
#include "url/url_canon.h"
#include "url/url_canon_stdstring.h"

namespace net {

HostResolverCache::HostResolverCache(const base::Clock& clock,
                                     const base::TickClock& tick_clock)
    : clock_(clock), tick_clock_(tick_clock) {}

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

    if (candidate->second.result->expiration().has_value()) {
      if (candidate->second.result->expiration() <= now_ticks) {
        continue;
      }
    } else if (candidate->second.result->timed_expiration() <= now) {
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

void HostResolverCache::Set(
    std::unique_ptr<HostResolverInternalResult> result,
    const NetworkAnonymizationKey& network_anonymization_key,
    HostResolverSource source,
    bool secure) {
  // Result must have at least a timed expiration to be a cacheable result.
  DCHECK(result->timed_expiration().has_value());

  std::vector<EntryMap::const_iterator> matches =
      LookupInternal(result->domain_name(), network_anonymization_key,
                     result->query_type(), source, secure);

  for (const EntryMap::const_iterator& match : matches) {
    entries_.erase(match);
  }

  std::string domain_name = result->domain_name();
  entries_.emplace(Key(std::move(domain_name), network_anonymization_key),
                   Entry(std::move(result), source, secure));
}

HostResolverCache::Entry::Entry(
    std::unique_ptr<HostResolverInternalResult> result,
    HostResolverSource source,
    bool secure)
    : result(std::move(result)), source(source), secure(secure) {}

HostResolverCache::Entry::~Entry() = default;

HostResolverCache::Entry::Entry(Entry&&) = default;

HostResolverCache::Entry& HostResolverCache::Entry::operator=(Entry&&) =
    default;

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

  auto range =
      entries_.equal_range(KeyRef{lookup_name, network_anonymization_key});
  if (range.first == entries_.cend() || range.second == entries_.cbegin()) {
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

}  // namespace net

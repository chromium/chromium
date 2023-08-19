// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_HOST_RESOLVER_CACHE_H_
#define NET_DNS_HOST_RESOLVER_CACHE_H_

#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/strings/string_piece.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/base/network_anonymization_key.h"
#include "net/dns/public/dns_query_type.h"
#include "net/dns/public/host_resolver_source.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {

class HostResolverInternalResult;

// Cache used by HostResolverManager to save previously resolved information.
class NET_EXPORT HostResolverCache final {
 public:
  struct StaleLookupResult {
    StaleLookupResult(const HostResolverInternalResult& result,
                      absl::optional<base::TimeDelta> expired_by,
                      bool stale_by_generation);
    ~StaleLookupResult() = default;

    const raw_ref<const HostResolverInternalResult> result;

    // Time since the result's TTL has expired. nullopt if not expired.
    const absl::optional<base::TimeDelta> expired_by;

    // True if result is stale due to a call to
    // HostResolverCache::MakeAllResultsStale().
    const bool stale_by_generation;

    bool IsStale() const {
      return stale_by_generation || expired_by.has_value();
    }
  };

  explicit HostResolverCache(
      size_t max_results,
      const base::Clock& clock = *base::DefaultClock::GetInstance(),
      const base::TickClock& tick_clock =
          *base::DefaultTickClock::GetInstance());
  ~HostResolverCache();

  // Move-only.
  HostResolverCache(HostResolverCache&&);
  HostResolverCache& operator=(HostResolverCache&&);

  // Lookup an active (non-stale) cached result matching the given criteria. If
  // `query_type` is `DnsQueryType::UNSPECIFIED`, `source` is
  // `HostResolverSource::ANY`, or `secure` is `absl::nullopt`, it is a wildcard
  // that can match for any cached parameter of that type. In cases where a
  // wildcard lookup leads to multiple matching results, only one result will be
  // returned, preferring first the most secure result and then the most
  // recently set one. Additionally, if a cached result has
  // `DnsQueryType::UNSPECIFIED`, it will match for any argument of
  // `query_type`.
  //
  // Returns nullptr on cache miss (no active result matches the given
  // criteria).
  const HostResolverInternalResult* Lookup(
      base::StringPiece domain_name,
      const NetworkAnonymizationKey& network_anonymization_key,
      DnsQueryType query_type = DnsQueryType::UNSPECIFIED,
      HostResolverSource source = HostResolverSource::ANY,
      absl::optional<bool> secure = absl::nullopt) const;

  // Lookup a cached result matching the given criteria. Unlike Lookup(), may
  // return stale results. In cases where a wildcard lookup leads to multiple
  // matching results, only one result will be returned, preferring active
  // (non-stale) results, then the least stale by generation, then the least
  // stale by time expiration, then the most secure, then the most recently set.
  //
  // Used to implement
  // `HostResolver::ResolveHostParameters::CacheUsage::STALE_ALLOWED` behavior,
  // which is itself primarily for usage by cronet::StaleHostResolver, but no
  // assumptions are made here that this is Cronet-only behavior.
  //
  // Returns nullopt on cache miss (no active or stale result matches the given
  // criteria).
  absl::optional<StaleLookupResult> LookupStale(
      base::StringPiece domain_name,
      const NetworkAnonymizationKey& network_anonymization_key,
      DnsQueryType query_type = DnsQueryType::UNSPECIFIED,
      HostResolverSource source = HostResolverSource::ANY,
      absl::optional<bool> secure = absl::nullopt) const;

  // Sets the result into the cache, replacing any previous result entries that
  // would match the same criteria, even if a previous entry would have matched
  // more criteria than the new one, e.g. if the previous entry used a wildcard
  // `DnsQueryType::UNSPECIFIED`.
  void Set(std::unique_ptr<HostResolverInternalResult> result,
           const NetworkAnonymizationKey& network_anonymization_key,
           HostResolverSource source,
           bool secure);

  // Makes all cached results considered stale. Typically used for network
  // change to ensure cached results are only considered active for the current
  // network.
  void MakeAllResultsStale();

 private:
  struct Key {
    ~Key();

    std::string domain_name;
    NetworkAnonymizationKey network_anonymization_key;
  };

  struct KeyRef {
    ~KeyRef() = default;

    base::StringPiece domain_name;
    const raw_ref<const NetworkAnonymizationKey> network_anonymization_key;
  };

  // Allow comparing Key to KeyRef to allow refs for entry lookup.
  struct KeyComparator {
    using is_transparent = void;

    ~KeyComparator() = default;

    bool operator()(const Key& lhs, const Key& rhs) const {
      return std::tie(lhs.domain_name, lhs.network_anonymization_key) <
             std::tie(rhs.domain_name, rhs.network_anonymization_key);
    }

    bool operator()(const Key& lhs, const KeyRef& rhs) const {
      return std::tie(lhs.domain_name, lhs.network_anonymization_key) <
             std::tie(rhs.domain_name, *rhs.network_anonymization_key);
    }

    bool operator()(const KeyRef& lhs, const Key& rhs) const {
      return std::tie(lhs.domain_name, *lhs.network_anonymization_key) <
             std::tie(rhs.domain_name, rhs.network_anonymization_key);
    }
  };

  struct Entry {
    Entry(std::unique_ptr<HostResolverInternalResult> result,
          HostResolverSource source,
          bool secure,
          int staleness_generation);
    ~Entry();

    Entry(Entry&&);
    Entry& operator=(Entry&&);

    bool IsStale(base::Time now,
                 base::TimeTicks now_ticks,
                 int current_staleness_generation) const;
    base::TimeDelta TimeUntilExpiration(base::Time now,
                                        base::TimeTicks now_ticks) const;

    std::unique_ptr<HostResolverInternalResult> result;
    HostResolverSource source;
    bool secure;

    // The `HostResolverCache::staleness_generation_` value at the time this
    // entry was created. Entry is stale if this does not match the current
    // value.
    int staleness_generation;
  };

  using EntryMap = std::multimap<Key, Entry, KeyComparator>;

  // Get all matching results, from most to least recently added.
  std::vector<EntryMap::const_iterator> LookupInternal(
      base::StringPiece domain_name,
      const NetworkAnonymizationKey& network_anonymization_key,
      DnsQueryType query_type,
      HostResolverSource source,
      absl::optional<bool> secure) const;

  void EvictEntries();

  EntryMap entries_;
  size_t max_entries_;

  // Number of times MakeAllEntriesStale() has been called.
  int staleness_generation_ = 0;

  raw_ref<const base::Clock> clock_;
  raw_ref<const base::TickClock> tick_clock_;
};

}  // namespace net

#endif  // NET_DNS_HOST_RESOLVER_CACHE_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_HOST_RESOLVER_CACHE_H_
#define NET_DNS_HOST_RESOLVER_CACHE_H_

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/base/net_export.h"
#include "net/base/network_anonymization_key.h"
#include "net/dns/public/dns_query_type.h"
#include "net/dns/public/host_resolver_source.h"

namespace net {

class HostResolverInternalResult;

// Cache used by HostResolverManager to save previously resolved information.
class NET_EXPORT HostResolverCache final {
 public:
  struct StaleLookupResult {
    StaleLookupResult(const HostResolverInternalResult& result,
                      std::optional<base::TimeDelta> expired_by,
                      bool stale_by_generation);
    ~StaleLookupResult() = default;

    const raw_ref<const HostResolverInternalResult> result;

    // Time since the result's TTL has expired. nullopt if not expired.
    const std::optional<base::TimeDelta> expired_by;

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
  // `HostResolverSource::ANY`, or `secure` is `std::nullopt`, it is a wildcard
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
      std::string_view domain_name,
      const NetworkAnonymizationKey& network_anonymization_key,
      DnsQueryType query_type = DnsQueryType::UNSPECIFIED,
      HostResolverSource source = HostResolverSource::ANY,
      std::optional<bool> secure = std::nullopt) const;

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
  std::optional<StaleLookupResult> LookupStale(
      std::string_view domain_name,
      const NetworkAnonymizationKey& network_anonymization_key,
      DnsQueryType query_type = DnsQueryType::UNSPECIFIED,
      HostResolverSource source = HostResolverSource::ANY,
      std::optional<bool> secure = std::nullopt) const;

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

  // Serialization to later be deserialized. Only serializes the results likely
  // to still be of value after serialization and deserialization, that is that
  // results with a transient anonymization key are not included.
  //
  // Used to implement cronet::HostCachePersistenceManager, but no assumptions
  // are made here that this is Cronet-only functionality.
  base::Value Serialize() const;

  // Deserialize value received from Serialize(). Results already contained in
  // the cache are preferred, thus deserialized results are ignored if any
  // previous result entries would match the same criteria, and deserialization
  // stops on reaching max size, rather than evicting anything. Deserialized
  // results are also always considered stale by generation.
  //
  // Returns false if `value` is malformed to be deserialized.
  //
  // Used to implement cronet::HostCachePersistenceManager, but no assumptions
  // are made here that this is Cronet-only functionality.
  bool RestoreFromValue(const base::Value& value);

  // Serialize for output to debug logs, e.g. netlog. Serializes all results,
  // including those with transient anonymization keys, and also serializes
  // cache-wide data. Incompatible with base::Values returned from Serialize(),
  // and cannot be used in RestoreFromValue().
  base::Value SerializeForLogging() const;

  bool AtMaxSizeForTesting() const { return entries_.size() >= max_entries_; }

 private:
  struct Key {
    ~Key();

    std::string domain_name;
    NetworkAnonymizationKey network_anonymization_key;
  };

  struct KeyRef {
    ~KeyRef() = default;

    std::string_view domain_name;
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
      std::string_view domain_name,
      const NetworkAnonymizationKey& network_anonymization_key,
      DnsQueryType query_type,
      HostResolverSource source,
      std::optional<bool> secure) const;

  void Set(std::unique_ptr<HostResolverInternalResult> result,
           const NetworkAnonymizationKey& network_anonymization_key,
           HostResolverSource source,
           bool secure,
           bool replace_existing,
           int staleness_generation);

  void EvictEntries();

  // If `require_persistable_anonymization_key` is true, will not serialize
  // any entries that do not have an anonymization key that supports
  // serialization and restoration. If false, will serialize all entries, but
  // the result may contain anonymization keys that are malformed for
  // restoration.
  base::Value SerializeEntries(
      bool serialize_staleness_generation,
      bool require_persistable_anonymization_key) const;

  EntryMap entries_;
  size_t max_entries_;

  // Number of times MakeAllEntriesStale() has been called.
  int staleness_generation_ = 0;

  raw_ref<const base::Clock> clock_;
  raw_ref<const base::TickClock> tick_clock_;
};

}  // namespace net

#endif  // NET_DNS_HOST_RESOLVER_CACHE_H_

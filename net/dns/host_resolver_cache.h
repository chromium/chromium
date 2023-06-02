// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_HOST_RESOLVER_CACHE_H_
#define NET_DNS_HOST_RESOLVER_CACHE_H_

#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/strings/string_piece.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
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
  explicit HostResolverCache(
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
  // wildcard lookup leads to multiple matching results, only the most recently
  // set result will be returned. Additionally, if a cached result has
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

  // Sets the result into the cache, replacing any previous result entries that
  // would match the same criteria, even if a previous entry would have matched
  // more criteria than the new one, e.g. if the previous entry used a wildcard
  // `DnsQueryType::UNSPECIFIED`.
  void Set(std::unique_ptr<HostResolverInternalResult> result,
           const NetworkAnonymizationKey& network_anonymization_key,
           HostResolverSource source,
           bool secure);

 private:
  struct Key {
    std::string domain_name;
    NetworkAnonymizationKey network_anonymization_key;
  };

  struct KeyRef {
    base::StringPiece domain_name;
    const NetworkAnonymizationKey& network_anonymization_key;
  };

  // Allow comparing Key to KeyRef to allow refs for entry lookup.
  struct KeyComparator {
    using is_transparent = void;

    bool operator()(const Key& lhs, const Key& rhs) const {
      return std::tie(lhs.domain_name, lhs.network_anonymization_key) <
             std::tie(rhs.domain_name, rhs.network_anonymization_key);
    }

    bool operator()(const Key& lhs, const KeyRef& rhs) const {
      return std::tie(lhs.domain_name, lhs.network_anonymization_key) <
             std::tie(rhs.domain_name, rhs.network_anonymization_key);
    }

    bool operator()(const KeyRef& lhs, const Key& rhs) const {
      return std::tie(lhs.domain_name, lhs.network_anonymization_key) <
             std::tie(rhs.domain_name, rhs.network_anonymization_key);
    }
  };

  struct Entry {
    Entry(std::unique_ptr<HostResolverInternalResult> result,
          HostResolverSource source,
          bool secure);
    ~Entry();

    Entry(Entry&&);
    Entry& operator=(Entry&&);

    std::unique_ptr<HostResolverInternalResult> result;
    HostResolverSource source;
    bool secure;
  };

  using EntryMap = std::multimap<Key, Entry, KeyComparator>;

  // Get all matching results, from most to least recently added.
  std::vector<EntryMap::const_iterator> LookupInternal(
      base::StringPiece domain_name,
      const NetworkAnonymizationKey& network_anonymization_key,
      DnsQueryType query_type,
      HostResolverSource source,
      absl::optional<bool> secure) const;

  EntryMap entries_;

  raw_ref<const base::Clock> clock_;
  raw_ref<const base::TickClock> tick_clock_;
};

}  // namespace net

#endif  // NET_DNS_HOST_RESOLVER_CACHE_H_

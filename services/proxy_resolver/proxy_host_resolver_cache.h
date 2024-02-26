// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PROXY_RESOLVER_PROXY_HOST_RESOLVER_CACHE_H_
#define SERVICES_PROXY_RESOLVER_PROXY_HOST_RESOLVER_CACHE_H_

#include <list>
#include <map>
#include <string>
#include <tuple>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "net/base/ip_address.h"
#include "net/base/network_anonymization_key.h"

namespace proxy_resolver {

// Simple cache for proxy host resolutions. Maintains cached entries for up to
// `kTtl`, and evicts oldest entries when filled past capacity.
class ProxyHostResolverCache {
 public:
  static constexpr auto kTtl = base::Seconds(5);

  explicit ProxyHostResolverCache(size_t max_entries = 500u);
  ~ProxyHostResolverCache();
  ProxyHostResolverCache(const ProxyHostResolverCache&) = delete;
  ProxyHostResolverCache& operator=(const ProxyHostResolverCache&) = delete;

  void StoreEntry(std::string hostname,
                  net::NetworkAnonymizationKey network_anonymization_key,
                  bool is_ex_operation,
                  std::vector<net::IPAddress> results);

  // Returns `nullptr` if entry not found or expired. Erases from cache if
  // expired.
  const std::vector<net::IPAddress>* LookupEntry(
      std::string hostname,
      net::NetworkAnonymizationKey network_anonymization_key,
      bool is_ex_operation);

  size_t GetSizeForTesting() const;

 private:
  struct Key {
    bool operator<(const Key& other) const {
      return std::tie(hostname, network_anonymization_key, is_ex_operation) <
             std::tie(other.hostname, other.network_anonymization_key,
                      other.is_ex_operation);
    }

    std::string hostname;
    net::NetworkAnonymizationKey network_anonymization_key;
    bool is_ex_operation;
  };

  using ExpirationList = std::list<raw_ptr<const Key, CtnExperimental>>;

  struct Entry {
    Entry(std::vector<net::IPAddress> results,
          base::TimeTicks expiration,
          ExpirationList::iterator expiration_list_it);
    ~Entry();
    Entry(Entry&&);
    Entry& operator=(Entry&&);

    std::vector<net::IPAddress> results;
    base::TimeTicks expiration;
    ExpirationList::iterator expiration_list_it;
  };

  using EntryMap = std::map<Key, Entry>;

  // Removes oldest entry iff `max_entries_` exceeded.
  void RemoveOldestEntry();

  size_t max_entries_;
  EntryMap entries_;

  // List of `const Key*`s in expiration order starting from the earliest to
  // expire.
  ExpirationList expiration_list_;
};

}  // namespace proxy_resolver

#endif  // SERVICES_PROXY_RESOLVER_PROXY_HOST_RESOLVER_CACHE_H_

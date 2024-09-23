// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/proxy_resolver/proxy_host_resolver_cache.h"

#include <list>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/not_fatal_until.h"
#include "base/time/time.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"

namespace proxy_resolver {

// static
constexpr base::TimeDelta ProxyHostResolverCache::kTtl;

ProxyHostResolverCache::ProxyHostResolverCache(size_t max_entries)
    : max_entries_(max_entries) {
  DCHECK_GT(max_entries_, 0u);
}

ProxyHostResolverCache::~ProxyHostResolverCache() = default;

void ProxyHostResolverCache::StoreEntry(
    std::string hostname,
    net::NetworkAnonymizationKey network_anonymization_key,
    bool is_ex_operation,
    std::vector<net::IPAddress> results) {
  Key key{std::move(hostname), std::move(network_anonymization_key),
          is_ex_operation};

  // Delete any old, now-obsolete entries.
  auto old_entry = entries_.find(key);
  if (old_entry != entries_.end()) {
    CHECK(old_entry->second.expiration_list_it != expiration_list_.end(),
          base::NotFatalUntil::M130);
    expiration_list_.erase(old_entry->second.expiration_list_it);
    entries_.erase(old_entry);
  }

  Entry entry(std::move(results),
              /*expiration=*/base::TimeTicks::Now() + kTtl,
              expiration_list_.end());
  auto entry_insertion_result =
      entries_.emplace(std::move(key), std::move(entry));
  DCHECK(entry_insertion_result.second);

  entry_insertion_result.first->second.expiration_list_it =
      expiration_list_.insert(expiration_list_.end(),
                              &entry_insertion_result.first->first);

  DCHECK_EQ(entries_.size(), expiration_list_.size());

  RemoveOldestEntry();
}

const std::vector<net::IPAddress>* ProxyHostResolverCache::LookupEntry(
    std::string hostname,
    net::NetworkAnonymizationKey network_anonymization_key,
    bool is_ex_operation) {
  Key key{std::move(hostname), std::move(network_anonymization_key),
          is_ex_operation};

  auto entry = entries_.find(key);
  if (entry == entries_.end())
    return nullptr;

  CHECK(entry->second.expiration_list_it != expiration_list_.end(),
        base::NotFatalUntil::M130);
  if (entry->second.expiration < base::TimeTicks::Now()) {
    expiration_list_.erase(std::move(entry->second.expiration_list_it));
    entries_.erase(entry);
    return nullptr;
  }

  return &entry->second.results;
}

size_t ProxyHostResolverCache::GetSizeForTesting() const {
  CHECK_EQ(entries_.size(), expiration_list_.size());
  return entries_.size();
}

ProxyHostResolverCache::Entry::Entry(
    std::vector<net::IPAddress> results,
    base::TimeTicks expiration,
    ExpirationList::iterator expiration_list_it)
    : results(std::move(results)),
      expiration(expiration),
      expiration_list_it(std::move(expiration_list_it)) {}

ProxyHostResolverCache::Entry::~Entry() = default;

ProxyHostResolverCache::Entry::Entry(Entry&&) = default;

ProxyHostResolverCache::Entry& ProxyHostResolverCache::Entry::operator=(
    Entry&&) = default;

void ProxyHostResolverCache::RemoveOldestEntry() {
  if (entries_.size() <= max_entries_)
    return;

  DCHECK_GT(expiration_list_.size(), 0u);
  size_t removed = entries_.erase(**expiration_list_.begin());
  DCHECK_EQ(removed, 1u);
  expiration_list_.pop_front();

  DCHECK_EQ(entries_.size(), expiration_list_.size());

  // Should be called at least after every individual insertion, so no more than
  // one removal should ever be needed.
  DCHECK_LE(entries_.size(), max_entries_);
}

}  // namespace proxy_resolver

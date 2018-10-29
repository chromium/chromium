// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/host_cache.h"

#include <utility>

#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/default_tick_clock.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "net/base/net_errors.h"
#include "net/base/trace_constants.h"
#include "net/dns/dns_util.h"
#include "net/log/net_log.h"

namespace net {

namespace {

#define CACHE_HISTOGRAM_TIME(name, time) \
  UMA_HISTOGRAM_LONG_TIMES("DNS.HostCache." name, time)

#define CACHE_HISTOGRAM_COUNT(name, count) \
  UMA_HISTOGRAM_COUNTS_1000("DNS.HostCache." name, count)

#define CACHE_HISTOGRAM_ENUM(name, value, max) \
  UMA_HISTOGRAM_ENUMERATION("DNS.HostCache." name, value, max)

// String constants for dictionary keys.
const char kHostnameKey[] = "hostname";
const char kAddressFamilyKey[] = "address_family";
const char kFlagsKey[] = "flags";
const char kExpirationKey[] = "expiration";
const char kTtlKey[] = "ttl";
const char kNetworkChangesKey[] = "network_changes";
const char kErrorKey[] = "error";
const char kAddressesKey[] = "addresses";

bool AddressListFromListValue(const base::ListValue* value, AddressList* list) {
  list->clear();
  for (auto it = value->begin(); it != value->end(); it++) {
    IPAddress address;
    std::string addr_string;
    if (!it->GetAsString(&addr_string) ||
        !address.AssignFromIPLiteral(addr_string)) {
      return false;
    }
    list->push_back(IPEndPoint(address, 0));
  }
  return true;
}

}  // namespace

// Used in histograms; do not modify existing values.
enum HostCache::SetOutcome : int {
  SET_INSERT = 0,
  SET_UPDATE_VALID = 1,
  SET_UPDATE_STALE = 2,
  MAX_SET_OUTCOME
};

// Used in histograms; do not modify existing values.
enum HostCache::LookupOutcome : int {
  LOOKUP_MISS_ABSENT = 0,
  LOOKUP_MISS_STALE = 1,
  LOOKUP_HIT_VALID = 2,
  LOOKUP_HIT_STALE = 3,
  MAX_LOOKUP_OUTCOME
};

// Used in histograms; do not modify existing values.
enum HostCache::EraseReason : int {
  ERASE_EVICT = 0,
  ERASE_CLEAR = 1,
  ERASE_DESTRUCT = 2,
  MAX_ERASE_REASON
};

HostCache::Entry::Entry(int error,
                        const AddressList& addresses,
                        Source source,
                        base::TimeDelta ttl)
    : error_(error), addresses_(addresses), source_(source), ttl_(ttl) {
  DCHECK(ttl >= base::TimeDelta());
}

HostCache::Entry::Entry(int error, const AddressList& addresses, Source source)
    : error_(error),
      addresses_(addresses),
      source_(source),
      ttl_(base::TimeDelta::FromSeconds(-1)) {}

HostCache::Entry::~Entry() = default;

HostCache::Entry::Entry(HostCache::Entry&& entry) = default;

HostCache::Entry::Entry(const HostCache::Entry& entry,
                        base::TimeTicks now,
                        base::TimeDelta ttl,
                        int network_changes)
    : error_(entry.error()),
      addresses_(entry.addresses()),
      source_(entry.source()),
      ttl_(entry.ttl()),
      expires_(now + ttl),
      network_changes_(network_changes),
      total_hits_(0),
      stale_hits_(0) {}

HostCache::Entry::Entry(int error,
                        const AddressList& addresses,
                        Source source,
                        base::TimeTicks expires,
                        int network_changes)
    : error_(error),
      addresses_(addresses),
      source_(source),
      ttl_(base::TimeDelta::FromSeconds(-1)),
      expires_(expires),
      network_changes_(network_changes),
      total_hits_(0),
      stale_hits_(0) {}

bool HostCache::Entry::IsStale(base::TimeTicks now, int network_changes) const {
  EntryStaleness stale;
  stale.expired_by = now - expires_;
  stale.network_changes = network_changes - network_changes_;
  stale.stale_hits = stale_hits_;
  return stale.is_stale();
}

void HostCache::Entry::CountHit(bool hit_is_stale) {
  ++total_hits_;
  if (hit_is_stale)
    ++stale_hits_;
}

void HostCache::Entry::GetStaleness(base::TimeTicks now,
                                    int network_changes,
                                    EntryStaleness* out) const {
  DCHECK(out);
  out->expired_by = now - expires_;
  out->network_changes = network_changes - network_changes_;
  out->stale_hits = stale_hits_;
}

HostCache::HostCache(size_t max_entries)
    : max_entries_(max_entries),
      network_changes_(0),
      restore_size_(0),
      delegate_(nullptr),
      tick_clock_(base::DefaultTickClock::GetInstance()) {}

HostCache::~HostCache() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  RecordEraseAll(ERASE_DESTRUCT, tick_clock_->NowTicks());
}

const HostCache::Entry* HostCache::Lookup(const Key& key,
                                          base::TimeTicks now) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (caching_is_disabled())
    return nullptr;

  HostCache::Entry* entry = LookupInternal(key);
  if (!entry) {
    RecordLookup(LOOKUP_MISS_ABSENT, now, nullptr);
    return nullptr;
  }
  if (entry->IsStale(now, network_changes_)) {
    RecordLookup(LOOKUP_MISS_STALE, now, entry);
    return nullptr;
  }

  entry->CountHit(/* hit_is_stale= */ false);
  RecordLookup(LOOKUP_HIT_VALID, now, entry);
  return entry;
}

const HostCache::Entry* HostCache::LookupStale(
    const Key& key,
    base::TimeTicks now,
    HostCache::EntryStaleness* stale_out) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (caching_is_disabled())
    return nullptr;

  HostCache::Entry* entry = LookupInternal(key);
  if (!entry) {
    RecordLookup(LOOKUP_MISS_ABSENT, now, nullptr);
    return nullptr;
  }

  bool is_stale = entry->IsStale(now, network_changes_);
  entry->CountHit(/* hit_is_stale= */ is_stale);
  RecordLookup(is_stale ? LOOKUP_HIT_STALE : LOOKUP_HIT_VALID, now, entry);

  if (stale_out)
    entry->GetStaleness(now, network_changes_, stale_out);
  return entry;
}

HostCache::Entry* HostCache::LookupInternal(const Key& key) {
  auto it = entries_.find(key);
  return (it != entries_.end()) ? &it->second : nullptr;
}

void HostCache::Set(const Key& key,
                    const Entry& entry,
                    base::TimeTicks now,
                    base::TimeDelta ttl) {
  TRACE_EVENT0(kNetTracingCategory, "HostCache::Set");
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (caching_is_disabled())
    return;

  bool result_changed = false;
  auto it = entries_.find(key);
  if (it != entries_.end()) {
    bool is_stale = it->second.IsStale(now, network_changes_);
    AddressListDeltaType delta =
        FindAddressListDeltaType(it->second.addresses(), entry.addresses());
    RecordSet(is_stale ? SET_UPDATE_STALE : SET_UPDATE_VALID, now, &it->second,
              entry, delta);
    // TODO(juliatuttle): Remember some old metadata (hit count or frequency or
    // something like that) if it's useful for better eviction algorithms?
    result_changed =
        entry.error() == OK &&
        (it->second.error() != entry.error() || delta != DELTA_IDENTICAL);
    entries_.erase(it);
  } else {
    result_changed = true;
    if (size() == max_entries_)
      EvictOneEntry(now);
    RecordSet(SET_INSERT, now, nullptr, entry, DELTA_DISJOINT);
  }

  AddEntry(Key(key), Entry(entry, now, ttl, network_changes_));

  if (delegate_ && result_changed)
    delegate_->ScheduleWrite();
}

void HostCache::AddEntry(const Key& key, Entry&& entry) {
  DCHECK_GT(max_entries_, size());
  DCHECK_EQ(0u, entries_.count(key));
  entries_.emplace(key, std::move(entry));
  DCHECK_GE(max_entries_, size());
}

void HostCache::OnNetworkChange() {
  ++network_changes_;
}

void HostCache::set_persistence_delegate(PersistenceDelegate* delegate) {
  // A PersistenceDelegate shouldn't be added if there already was one, and
  // shouldn't be removed (by setting to nullptr) if it wasn't previously there.
  DCHECK_NE(delegate == nullptr, delegate_ == nullptr);
  delegate_ = delegate;
}

void HostCache::clear() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  RecordEraseAll(ERASE_CLEAR, tick_clock_->NowTicks());

  // Don't bother scheduling a write if there's nothing to clear.
  if (size() == 0)
    return;

  entries_.clear();
  if (delegate_)
    delegate_->ScheduleWrite();
}

void HostCache::ClearForHosts(
    const base::Callback<bool(const std::string&)>& host_filter) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (host_filter.is_null()) {
    clear();
    return;
  }

  bool changed = false;
  base::TimeTicks now = tick_clock_->NowTicks();
  for (auto it = entries_.begin(); it != entries_.end();) {
    auto next_it = std::next(it);

    if (host_filter.Run(it->first.hostname)) {
      RecordErase(ERASE_CLEAR, now, it->second);
      entries_.erase(it);
      changed = true;
    }

    it = next_it;
  }

  if (delegate_ && changed)
    delegate_->ScheduleWrite();
}

void HostCache::GetAsListValue(base::ListValue* entry_list,
                               bool include_staleness) const {
  DCHECK(entry_list);
  entry_list->Clear();

  for (const auto& pair : entries_) {
    const Key& key = pair.first;
    const Entry& entry = pair.second;

    std::unique_ptr<base::DictionaryValue> entry_dict(
        new base::DictionaryValue());

    entry_dict->SetString(kHostnameKey, key.hostname);
    entry_dict->SetInteger(kAddressFamilyKey,
                           static_cast<int>(key.address_family));
    entry_dict->SetInteger(kFlagsKey, key.host_resolver_flags);

    if (include_staleness) {
      // The kExpirationKey value is using TimeTicks instead of Time used if
      // |include_staleness| is false, so it cannot be used to deserialize.
      // This is ok as it is used only for netlog.
      entry_dict->SetString(kExpirationKey,
                            NetLog::TickCountToString(entry.expires()));
      entry_dict->SetInteger(kTtlKey, entry.ttl().InMilliseconds());
      entry_dict->SetInteger(kNetworkChangesKey, entry.network_changes());
    } else {
      // Convert expiration time in TimeTicks to Time for serialization, using a
      // string because base::Value doesn't handle 64-bit integers.
      base::Time expiration_time =
          base::Time::Now() - (tick_clock_->NowTicks() - entry.expires());
      entry_dict->SetString(
          kExpirationKey,
          base::Int64ToString(expiration_time.ToInternalValue()));
    }

    if (entry.error() != OK) {
      entry_dict->SetInteger(kErrorKey, entry.error());
    } else {
      const AddressList& addresses = entry.addresses();
      // Append all of the resolved addresses.
      auto addresses_value = std::make_unique<base::ListValue>();
      for (size_t i = 0; i < addresses.size(); ++i)
        addresses_value->AppendString(addresses[i].ToStringWithoutPort());
      entry_dict->SetList(kAddressesKey, std::move(addresses_value));
    }

    entry_list->Append(std::move(entry_dict));
  }
}

bool HostCache::RestoreFromListValue(const base::ListValue& old_cache) {
  for (auto it = old_cache.begin(); it != old_cache.end(); it++) {
    const base::DictionaryValue* entry_dict;
    if (!it->GetAsDictionary(&entry_dict))
      return false;

    std::string hostname;
    int address_family;
    HostResolverFlags flags;
    int error = OK;
    std::string expiration;
    base::ListValue empty_list;
    const base::ListValue* addresses_value = &empty_list;
    AddressList address_list;

    if (!entry_dict->GetString(kHostnameKey, &hostname) ||
        !entry_dict->GetInteger(kFlagsKey, &flags) ||
        !entry_dict->GetInteger(kAddressFamilyKey, &address_family) ||
        !entry_dict->GetString(kExpirationKey, &expiration)) {
      return false;
    }

    // Only one of these fields should be in the dictionary.
    if (!entry_dict->GetInteger(kErrorKey, &error) &&
        !entry_dict->GetList(kAddressesKey, &addresses_value)) {
      return false;
    }

    int64_t time_internal;
    if (!base::StringToInt64(expiration, &time_internal))
      return false;

    base::TimeTicks expiration_time =
        tick_clock_->NowTicks() -
        (base::Time::Now() - base::Time::FromInternalValue(time_internal));

    Key key(hostname, static_cast<AddressFamily>(address_family), flags);
    if (error == OK &&
        !AddressListFromListValue(addresses_value, &address_list)) {
      return false;
    }

    // If the key is already in the cache, assume it's more recent and don't
    // replace the entry. If the cache is already full, don't bother
    // prioritizing what to evict, just stop restoring.
    auto found = entries_.find(key);
    if (found == entries_.end() && size() < max_entries_) {
      AddEntry(key, Entry(error, address_list, Entry::SOURCE_UNKNOWN,
                          expiration_time, network_changes_ - 1));
    }
  }
  restore_size_ = old_cache.GetSize();
  return true;
}

size_t HostCache::size() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return entries_.size();
}

size_t HostCache::max_entries() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return max_entries_;
}

// static
std::unique_ptr<HostCache> HostCache::CreateDefaultCache() {
#if defined(ENABLE_BUILT_IN_DNS)
  const size_t kDefaultMaxEntries = 1000;
#else
  const size_t kDefaultMaxEntries = 100;
#endif
  return std::make_unique<HostCache>(kDefaultMaxEntries);
}

void HostCache::EvictOneEntry(base::TimeTicks now) {
  DCHECK_LT(0u, entries_.size());

  auto oldest_it = entries_.begin();
  for (auto it = entries_.begin(); it != entries_.end(); ++it) {
    if ((it->second.expires() < oldest_it->second.expires()) &&
        (it->second.IsStale(now, network_changes_) ||
         !oldest_it->second.IsStale(now, network_changes_))) {
      oldest_it = it;
    }
  }

  RecordErase(ERASE_EVICT, now, oldest_it->second);
  entries_.erase(oldest_it);
}

void HostCache::RecordSet(SetOutcome outcome,
                          base::TimeTicks now,
                          const Entry* old_entry,
                          const Entry& new_entry,
                          AddressListDeltaType delta) {
  CACHE_HISTOGRAM_ENUM("Set", outcome, MAX_SET_OUTCOME);
  switch (outcome) {
    case SET_INSERT:
    case SET_UPDATE_VALID:
      // Nothing to log here.
      break;
    case SET_UPDATE_STALE: {
      EntryStaleness stale;
      old_entry->GetStaleness(now, network_changes_, &stale);
      CACHE_HISTOGRAM_TIME("UpdateStale.ExpiredBy", stale.expired_by);
      CACHE_HISTOGRAM_COUNT("UpdateStale.NetworkChanges",
                            stale.network_changes);
      CACHE_HISTOGRAM_COUNT("UpdateStale.StaleHits", stale.stale_hits);
      if (old_entry->error() == OK && new_entry.error() == OK) {
        RecordUpdateStale(delta, stale);
      }
      break;
    }
    case MAX_SET_OUTCOME:
      NOTREACHED();
      break;
  }
}

void HostCache::RecordUpdateStale(AddressListDeltaType delta,
                                  const EntryStaleness& stale) {
  CACHE_HISTOGRAM_ENUM("UpdateStale.AddressListDelta", delta, MAX_DELTA_TYPE);
  switch (delta) {
    case DELTA_IDENTICAL:
      CACHE_HISTOGRAM_TIME("UpdateStale.ExpiredBy_Identical", stale.expired_by);
      CACHE_HISTOGRAM_COUNT("UpdateStale.NetworkChanges_Identical",
                            stale.network_changes);
      break;
    case DELTA_REORDERED:
      CACHE_HISTOGRAM_TIME("UpdateStale.ExpiredBy_Reordered", stale.expired_by);
      CACHE_HISTOGRAM_COUNT("UpdateStale.NetworkChanges_Reordered",
                            stale.network_changes);
      break;
    case DELTA_OVERLAP:
      CACHE_HISTOGRAM_TIME("UpdateStale.ExpiredBy_Overlap", stale.expired_by);
      CACHE_HISTOGRAM_COUNT("UpdateStale.NetworkChanges_Overlap",
                            stale.network_changes);
      break;
    case DELTA_DISJOINT:
      CACHE_HISTOGRAM_TIME("UpdateStale.ExpiredBy_Disjoint", stale.expired_by);
      CACHE_HISTOGRAM_COUNT("UpdateStale.NetworkChanges_Disjoint",
                            stale.network_changes);
      break;
    case MAX_DELTA_TYPE:
      NOTREACHED();
      break;
  }
}

void HostCache::RecordLookup(LookupOutcome outcome,
                             base::TimeTicks now,
                             const Entry* entry) {
  CACHE_HISTOGRAM_ENUM("Lookup", outcome, MAX_LOOKUP_OUTCOME);
  switch (outcome) {
    case LOOKUP_MISS_ABSENT:
    case LOOKUP_MISS_STALE:
    case LOOKUP_HIT_VALID:
      // Nothing to log here.
      break;
    case LOOKUP_HIT_STALE:
      CACHE_HISTOGRAM_TIME("LookupStale.ExpiredBy", now - entry->expires());
      CACHE_HISTOGRAM_COUNT("LookupStale.NetworkChanges",
                            network_changes_ - entry->network_changes());
      break;
    case MAX_LOOKUP_OUTCOME:
      NOTREACHED();
      break;
  }
}

void HostCache::RecordErase(EraseReason reason,
                            base::TimeTicks now,
                            const Entry& entry) {
  HostCache::EntryStaleness stale;
  entry.GetStaleness(now, network_changes_, &stale);
  CACHE_HISTOGRAM_ENUM("Erase", reason, MAX_ERASE_REASON);
  if (stale.is_stale()) {
    CACHE_HISTOGRAM_TIME("EraseStale.ExpiredBy", stale.expired_by);
    CACHE_HISTOGRAM_COUNT("EraseStale.NetworkChanges", stale.network_changes);
    CACHE_HISTOGRAM_COUNT("EraseStale.StaleHits", entry.stale_hits());
  } else {
    CACHE_HISTOGRAM_TIME("EraseValid.ValidFor", -stale.expired_by);
  }
}

void HostCache::RecordEraseAll(EraseReason reason, base::TimeTicks now) {
  for (const auto& it : entries_)
    RecordErase(reason, now, it.second);
}

bool HostCache::HasEntry(base::StringPiece hostname,
                         HostCache::Entry::Source* source_out,
                         HostCache::EntryStaleness* stale_out) {
  net::HostCache::Key cache_key;
  hostname.CopyToString(&cache_key.hostname);

  const HostCache::Entry* entry =
      LookupStale(cache_key, tick_clock_->NowTicks(), stale_out);
  if (!entry) {
    // Might not have found the cache entry because the address_family or
    // host_resolver_flags in cache_key do not match those used for the
    // original DNS lookup. Try another common combination of address_family
    // and host_resolver_flags in an attempt to find a matching cache entry.
    cache_key.address_family = net::ADDRESS_FAMILY_IPV4;
    cache_key.host_resolver_flags =
        net::HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6;
    entry = LookupStale(cache_key, tick_clock_->NowTicks(), stale_out);
    if (!entry)
      return false;
  }

  if (source_out != nullptr)
    *source_out = entry->source();

  return true;
}

}  // namespace net

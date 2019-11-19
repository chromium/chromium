// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/host_cache.h"

#include <algorithm>

#include "base/bind.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/default_tick_clock.h"
#include "base/trace_event/trace_event.h"
#include "net/base/ip_endpoint.h"
#include "net/base/trace_constants.h"
#include "net/dns/host_resolver.h"
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
const char kDnsQueryTypeKey[] = "dns_query_type";
const char kFlagsKey[] = "flags";
const char kHostResolverSourceKey[] = "host_resolver_source";
const char kSecureKey[] = "secure";
const char kNetworkIsolationKeyKey[] = "network_isolation_key";
const char kExpirationKey[] = "expiration";
const char kTtlKey[] = "ttl";
const char kNetworkChangesKey[] = "network_changes";
const char kErrorKey[] = "error";
const char kAddressesKey[] = "addresses";
const char kTextRecordsKey[] = "text_records";
const char kHostnameResultsKey[] = "hostname_results";
const char kHostPortsKey[] = "host_ports";
const char kEsniDataKey[] = "esni_data";

bool AddressListFromListValue(const base::ListValue* value,
                              base::Optional<AddressList>* out_list) {
  if (!value) {
    out_list->reset();
    return true;
  }

  out_list->emplace();
  for (auto it = value->begin(); it != value->end(); it++) {
    IPAddress address;
    std::string addr_string;
    if (!it->GetAsString(&addr_string) ||
        !address.AssignFromIPLiteral(addr_string)) {
      return false;
    }
    out_list->value().push_back(IPEndPoint(address, 0));
  }
  return true;
}

// Serializes the cache's ESNI content as
// {
//   key 0: [addresses for key 0],
//   ...,
//   key N: [address for key N]
// }
base::Value EsniContentToValue(const EsniContent& content) {
  base::Value addresses_for_keys_value(base::Value::Type::DICTIONARY);

  for (const auto& key : content.keys()) {
    addresses_for_keys_value.SetKey(key, base::Value(base::Value::Type::LIST));
  }

  for (const auto& kv : content.keys_for_addresses()) {
    const IPAddress& address = kv.first;
    const auto& keys_for_address = kv.second;
    for (base::StringPiece key : keys_for_address) {
      base::Value* addresses_for_key = addresses_for_keys_value.FindKey(key);
      DCHECK(addresses_for_key);
      addresses_for_key->Append(address.ToString());
    }
  }

  return addresses_for_keys_value;
}

bool EsniContentFromValue(const base::Value& esni_content_value,
                          base::Optional<EsniContent>* out_esni_content) {
  EsniContent content_for_cache;

  // The esni_data cache member is encoded as a
  // { key: list of associated IP addresses } dictionary.
  if (!esni_content_value.is_dict())
    return false;

  for (const auto& kv : esni_content_value.DictItems()) {
    const std::string& key = kv.first;
    const base::Value& serialized_addresses = kv.second;
    if (!serialized_addresses.is_list())
      return false;
    if (serialized_addresses.GetList().empty()) {
      content_for_cache.AddKey(key);
    } else {
      for (const base::Value& serialized_address_value :
           serialized_addresses.GetList()) {
        if (!serialized_address_value.is_string())
          return false;
        const std::string& serialized_address =
            serialized_address_value.GetString();
        IPAddress address;
        if (!address.AssignFromIPLiteral(serialized_address))
          return false;
        content_for_cache.AddKeyForAddress(address, key);
      }
    }
  }

  *out_esni_content = std::move(content_for_cache);
  return true;
}

template <typename T>
void MergeLists(base::Optional<T>* target, const base::Optional<T>& source) {
  if (target->has_value() && source) {
    target->value().insert(target->value().end(), source.value().begin(),
                           source.value().end());
  } else if (source) {
    *target = source;
  }
}

class InvalidatorImpl : public HostCache::Invalidator {
 public:
  explicit InvalidatorImpl(HostCache* cache) : cache_(cache) {}
  void Invalidate() override { cache_->Invalidate(); }

 private:
  HostCache* cache_;

  DISALLOW_COPY_AND_ASSIGN(InvalidatorImpl);
};

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

HostCache::Key::Key(const std::string& hostname,
                    DnsQueryType dns_query_type,
                    HostResolverFlags host_resolver_flags,
                    HostResolverSource host_resolver_source,
                    const NetworkIsolationKey& network_isolation_key)
    : hostname(hostname),
      dns_query_type(dns_query_type),
      host_resolver_flags(host_resolver_flags),
      host_resolver_source(host_resolver_source),
      network_isolation_key(network_isolation_key) {}

HostCache::Key::Key() = default;
HostCache::Key::Key(const Key& key) = default;
HostCache::Key::Key(Key&& key) = default;

HostCache::Entry::Entry(int error, Source source, base::TimeDelta ttl)
    : error_(error), source_(source), ttl_(ttl) {
  DCHECK_GE(ttl_, base::TimeDelta());
  DCHECK_NE(OK, error_);
}

HostCache::Entry::Entry(int error, Source source)
    : error_(error), source_(source), ttl_(base::TimeDelta::FromSeconds(-1)) {
  DCHECK_NE(OK, error_);
}

HostCache::Entry::Entry(const Entry& entry) = default;

HostCache::Entry::Entry(Entry&& entry) = default;

HostCache::Entry::~Entry() = default;

base::Optional<base::TimeDelta> HostCache::Entry::GetOptionalTtl() const {
  if (has_ttl())
    return ttl();
  else
    return base::nullopt;
}

// static
HostCache::Entry HostCache::Entry::MergeEntries(Entry front, Entry back) {
  // Only expected to merge OK or ERR_NAME_NOT_RESOLVED results.
  DCHECK(front.error() == OK || front.error() == ERR_NAME_NOT_RESOLVED);
  DCHECK(back.error() == OK || back.error() == ERR_NAME_NOT_RESOLVED);

  // Build results in |front| to preserve unmerged fields.

  front.error_ =
      front.error() == OK || back.error() == OK ? OK : ERR_NAME_NOT_RESOLVED;

  front.MergeAddressesFrom(back);
  MergeLists(&front.text_records_, back.text_records());
  MergeLists(&front.hostnames_, back.hostnames());
  if (back.esni_data_ && !front.esni_data_) {
    front.esni_data_ = std::move(back.esni_data_);
  } else if (front.esni_data_ && back.esni_data_) {
    front.esni_data_->MergeFrom(back.esni_data_.value());
  }

  // Use canonical name from |back| iff empty in |front|.
  if (front.addresses() && front.addresses().value().canonical_name().empty() &&
      back.addresses()) {
    front.addresses_.value().set_canonical_name(
        back.addresses().value().canonical_name());
  }

  // Only expected to merge entries from same source.
  DCHECK_EQ(front.source(), back.source());

  if (front.has_ttl() && back.has_ttl()) {
    front.ttl_ = std::min(front.ttl(), back.ttl());
  } else if (back.has_ttl()) {
    front.ttl_ = back.ttl();
  }

  front.expires_ = std::min(front.expires(), back.expires());
  front.network_changes_ =
      std::max(front.network_changes(), back.network_changes());

  front.total_hits_ = front.total_hits_ + back.total_hits_;
  front.stale_hits_ = front.stale_hits_ + back.stale_hits_;

  return front;
}

HostCache::Entry HostCache::Entry::CopyWithDefaultPort(uint16_t port) const {
  Entry copy(*this);

  if (addresses() &&
      std::any_of(addresses().value().begin(), addresses().value().end(),
                  [](const IPEndPoint& e) { return e.port() == 0; })) {
    AddressList addresses_with_port;
    addresses_with_port.set_canonical_name(
        addresses().value().canonical_name());
    for (const IPEndPoint& endpoint : addresses().value()) {
      if (endpoint.port() == 0)
        addresses_with_port.push_back(IPEndPoint(endpoint.address(), port));
      else
        addresses_with_port.push_back(endpoint);
    }
    copy.set_addresses(addresses_with_port);
  }

  if (hostnames() &&
      std::any_of(hostnames().value().begin(), hostnames().value().end(),
                  [](const HostPortPair& h) { return h.port() == 0; })) {
    std::vector<HostPortPair> hostnames_with_port;
    for (const HostPortPair& hostname : hostnames().value()) {
      if (hostname.port() == 0)
        hostnames_with_port.push_back(HostPortPair(hostname.host(), port));
      else
        hostnames_with_port.push_back(hostname);
    }
    copy.set_hostnames(std::move(hostnames_with_port));
  }

  return copy;
}

HostCache::Entry& HostCache::Entry::operator=(const Entry& entry) = default;

HostCache::Entry& HostCache::Entry::operator=(Entry&& entry) = default;

HostCache::Entry::Entry(const HostCache::Entry& entry,
                        base::TimeTicks now,
                        base::TimeDelta ttl,
                        int network_changes)
    : error_(entry.error()),
      addresses_(entry.addresses()),
      text_records_(entry.text_records()),
      hostnames_(entry.hostnames()),
      esni_data_(entry.esni_data()),
      source_(entry.source()),
      ttl_(entry.ttl()),
      expires_(now + ttl),
      network_changes_(network_changes) {}

HostCache::Entry::Entry(int error,
                        const base::Optional<AddressList>& addresses,
                        base::Optional<std::vector<std::string>>&& text_records,
                        base::Optional<std::vector<HostPortPair>>&& hostnames,
                        base::Optional<EsniContent>&& esni_data,
                        Source source,
                        base::TimeTicks expires,
                        int network_changes)
    : error_(error),
      addresses_(addresses),
      text_records_(std::move(text_records)),
      hostnames_(std::move(hostnames)),
      esni_data_(std::move(esni_data)),
      source_(source),
      expires_(expires),
      network_changes_(network_changes) {}

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

base::Value HostCache::Entry::NetLogParams() const {
  return GetAsValue(false /* include_staleness */);
}

void HostCache::Entry::MergeAddressesFrom(const HostCache::Entry& source) {
  MergeLists(&addresses_, source.addresses());
  if (!addresses_ || addresses_->size() <= 1)
    return;  // Nothing to do.

  addresses_->Deduplicate();

  auto has_keys = [&](const IPEndPoint& e) {
    return (esni_data() &&
            esni_data()->keys_for_addresses().count(e.address())) ||
           (source.esni_data() &&
            source.esni_data()->keys_for_addresses().count(e.address()));
  };

  std::stable_sort(addresses_->begin(), addresses_->end(),
                   [&](const IPEndPoint& lhs, const IPEndPoint& rhs) {
                     // Prefer an address with ESNI keys to one without;
                     // break ties by address family.

                     // Store one lookup's result to avoid repeating the lookup.
                     bool lhs_has_keys = has_keys(lhs);
                     if (lhs_has_keys != has_keys(rhs))
                       return lhs_has_keys;

                     if ((lhs.GetFamily() == ADDRESS_FAMILY_IPV6) !=
                         (rhs.GetFamily() == ADDRESS_FAMILY_IPV6))
                       return (lhs.GetFamily() == ADDRESS_FAMILY_IPV6);

                     return false;
                   });
}

base::DictionaryValue HostCache::Entry::GetAsValue(
    bool include_staleness) const {
  base::DictionaryValue entry_dict;

  if (include_staleness) {
    // The kExpirationKey value is using TimeTicks instead of Time used if
    // |include_staleness| is false, so it cannot be used to deserialize.
    // This is ok as it is used only for netlog.
    entry_dict.SetString(kExpirationKey, NetLog::TickCountToString(expires()));
    entry_dict.SetInteger(kTtlKey, ttl().InMilliseconds());
    entry_dict.SetInteger(kNetworkChangesKey, network_changes());
  } else {
    // Convert expiration time in TimeTicks to Time for serialization, using a
    // string because base::Value doesn't handle 64-bit integers.
    base::Time expiration_time =
        base::Time::Now() - (base::TimeTicks::Now() - expires());
    entry_dict.SetString(
        kExpirationKey,
        base::NumberToString(expiration_time.ToInternalValue()));
  }

  if (error() != OK) {
    entry_dict.SetInteger(kErrorKey, error());
  } else {
    if (addresses()) {
      // Append all of the resolved addresses.
      base::ListValue addresses_value;
      for (const IPEndPoint& address : addresses().value()) {
        addresses_value.Append(address.ToStringWithoutPort());
      }
      entry_dict.SetKey(kAddressesKey, std::move(addresses_value));
    }

    if (text_records()) {
      // Append all resolved text records.
      base::ListValue text_list_value;
      for (const std::string& text_record : text_records().value()) {
        text_list_value.Append(text_record);
      }
      entry_dict.SetKey(kTextRecordsKey, std::move(text_list_value));
    }

    if (hostnames()) {
      // Append all the resolved hostnames.
      base::ListValue hostnames_value;
      base::ListValue host_ports_value;
      for (const HostPortPair& hostname : hostnames().value()) {
        hostnames_value.Append(hostname.host());
        host_ports_value.Append(hostname.port());
      }
      entry_dict.SetKey(kHostnameResultsKey, std::move(hostnames_value));
      entry_dict.SetKey(kHostPortsKey, std::move(host_ports_value));
    }

    if (esni_data()) {
      entry_dict.SetKey(kEsniDataKey, EsniContentToValue(*esni_data()));
    }
  }

  return entry_dict;
}

// static
const HostCache::EntryStaleness HostCache::kNotStale = {
    base::TimeDelta::FromSeconds(-1), 0, 0};

HostCache::HostCache(size_t max_entries)
    : max_entries_(max_entries),
      network_changes_(0),
      restore_size_(0),
      delegate_(nullptr),
      tick_clock_(base::DefaultTickClock::GetInstance()),
      owned_invalidator_(std::make_unique<InvalidatorImpl>(this)),
      invalidator_(owned_invalidator_.get()) {}

HostCache::~HostCache() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

const std::pair<const HostCache::Key, HostCache::Entry>*
HostCache::Lookup(const Key& key, base::TimeTicks now, bool ignore_secure) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (caching_is_disabled())
    return nullptr;

  auto* result = LookupInternalIgnoringFields(key, now, ignore_secure);
  if (!result)
    return nullptr;

  auto* entry = &result->second;
  if (entry->IsStale(now, network_changes_))
    return nullptr;

  entry->CountHit(/* hit_is_stale= */ false);
  return result;
}

const std::pair<const HostCache::Key, HostCache::Entry>* HostCache::LookupStale(
    const Key& key,
    base::TimeTicks now,
    HostCache::EntryStaleness* stale_out,
    bool ignore_secure) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (caching_is_disabled())
    return nullptr;

  auto* result = LookupInternalIgnoringFields(key, now, ignore_secure);
  if (!result)
    return nullptr;

  auto* entry = &result->second;
  bool is_stale = entry->IsStale(now, network_changes_);
  entry->CountHit(/* hit_is_stale= */ is_stale);

  if (stale_out)
    entry->GetStaleness(now, network_changes_, stale_out);
  return result;
}

// static
std::pair<const HostCache::Key, HostCache::Entry>*
HostCache::GetLessStaleMoreSecureResult(
    base::TimeTicks now,
    std::pair<const HostCache::Key, HostCache::Entry>* result1,
    std::pair<const HostCache::Key, HostCache::Entry>* result2) {
  // Prefer a non-null result if possible.
  if (!result1 && !result2)
    return nullptr;
  if (result1 && !result2)
    return result1;
  if (!result1 && result2)
    return result2;

  // Both result1 are result2 are non-null.
  EntryStaleness staleness1, staleness2;
  result1->second.GetStaleness(now, 0, &staleness1);
  result2->second.GetStaleness(now, 0, &staleness2);
  if (staleness1.network_changes == staleness2.network_changes) {
    // Exactly one of the results should be secure.
    DCHECK(result1->first.secure != result2->first.secure);
    // If the results have the same number of network changes, prefer a
    // non-expired result.
    if (staleness1.expired_by < base::TimeDelta() &&
        staleness2.expired_by >= base::TimeDelta()) {
      return result1;
    }
    if (staleness1.expired_by >= base::TimeDelta() &&
        staleness2.expired_by < base::TimeDelta()) {
      return result2;
    }
    // Both results are equally stale, so prefer a secure result.
    return (result1->first.secure) ? result1 : result2;
  }
  // Prefer the result with the fewest network changes.
  return (staleness1.network_changes < staleness2.network_changes) ? result1
                                                                   : result2;
}

std::pair<const HostCache::Key, HostCache::Entry>*
HostCache::LookupInternalIgnoringFields(const Key& initial_key,
                                        base::TimeTicks now,
                                        bool ignore_secure) {
  std::pair<const HostCache::Key, HostCache::Entry>* preferred_result =
      LookupInternal(initial_key);

  if (ignore_secure) {
    Key effective_key = initial_key;
    effective_key.secure = !initial_key.secure;
    preferred_result = GetLessStaleMoreSecureResult(
        now, preferred_result, LookupInternal(effective_key));
  }

  return preferred_result;
}

std::pair<const HostCache::Key, HostCache::Entry>* HostCache::LookupInternal(
    const Key& key) {
  auto it = entries_.find(key);
  return (it != entries_.end()) ? &*it : nullptr;
}

void HostCache::Set(const Key& key,
                    const Entry& entry,
                    base::TimeTicks now,
                    base::TimeDelta ttl) {
  TRACE_EVENT0(NetTracingCategory(), "HostCache::Set");
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (caching_is_disabled())
    return;

  bool result_changed = false;
  auto it = entries_.find(key);
  if (it != entries_.end()) {
    base::Optional<AddressListDeltaType> addresses_delta;
    if (entry.addresses() || it->second.addresses()) {
      if (entry.addresses() && it->second.addresses()) {
        addresses_delta = FindAddressListDeltaType(
            it->second.addresses().value(), entry.addresses().value());
      } else {
        addresses_delta = DELTA_DISJOINT;
      }
    }  // Else no addresses in old or new, so nullopt delta.

    // For non-address results, delta is only considered for whole-list
    // equality. The meaning of partial list equality varies too much depending
    // on the context of a DNS record.
    base::Optional<AddressListDeltaType> nonaddress_delta;
    if (entry.text_records() || it->second.text_records() ||
        entry.hostnames() || it->second.hostnames()) {
      if (entry.text_records() == it->second.text_records() &&
          entry.hostnames() == it->second.hostnames()) {
        nonaddress_delta = DELTA_IDENTICAL;
      } else if (entry.text_records() == it->second.text_records() ||
                 entry.hostnames() == it->second.hostnames()) {
        nonaddress_delta = DELTA_OVERLAP;
      } else {
        nonaddress_delta = DELTA_DISJOINT;
      }
    }  // Else no nonaddress results in old or new, so nullopt delta.

    AddressListDeltaType overall_delta;
    if (!addresses_delta && !nonaddress_delta) {
      // No results in old or new is IDENTICAL.
      overall_delta = DELTA_IDENTICAL;
    } else if (!addresses_delta) {
      overall_delta = nonaddress_delta.value();
    } else if (!nonaddress_delta) {
      overall_delta = addresses_delta.value();
    } else if (addresses_delta == DELTA_DISJOINT &&
               nonaddress_delta == DELTA_DISJOINT) {
      overall_delta = DELTA_DISJOINT;
    } else if (addresses_delta == DELTA_DISJOINT ||
               nonaddress_delta == DELTA_DISJOINT) {
      // If only some result types are DISJOINT, some match and we have OVERLAP.
      overall_delta = DELTA_OVERLAP;
    } else {
      // No DISJOINT result types, so we have at least partial match.  Take the
      // least matching amount (highest enum value).
      overall_delta =
          std::max(addresses_delta.value(), nonaddress_delta.value());
    }

    // TODO(juliatuttle): Remember some old metadata (hit count or frequency or
    // something like that) if it's useful for better eviction algorithms?
    result_changed =
        entry.error() == OK && (it->second.error() != entry.error() ||
                                overall_delta != DELTA_IDENTICAL);
    entries_.erase(it);
  } else {
    result_changed = true;
    if (size() == max_entries_)
      EvictOneEntry(now);
  }

  AddEntry(key, Entry(entry, now, ttl, network_changes_));

  if (delegate_ && result_changed)
    delegate_->ScheduleWrite();
}

void HostCache::AddEntry(const Key& key, Entry&& entry) {
  DCHECK_GT(max_entries_, size());
  DCHECK_EQ(0u, entries_.count(key));
  entries_.emplace(key, std::move(entry));
  DCHECK_GE(max_entries_, size());
}

void HostCache::Invalidate() {
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
  for (auto it = entries_.begin(); it != entries_.end();) {
    auto next_it = std::next(it);

    if (host_filter.Run(it->first.hostname)) {
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

    base::Value network_isolation_key_value;
    // Don't save entries associated with ephemeral NetworkIsolationKeys.
    if (!key.network_isolation_key.ToValue(&network_isolation_key_value))
      continue;

    auto entry_dict = std::make_unique<base::DictionaryValue>(
        entry.GetAsValue(include_staleness));

    entry_dict->SetString(kHostnameKey, key.hostname);
    entry_dict->SetInteger(kDnsQueryTypeKey,
                           static_cast<int>(key.dns_query_type));
    entry_dict->SetInteger(kFlagsKey, key.host_resolver_flags);
    entry_dict->SetInteger(kHostResolverSourceKey,
                           static_cast<int>(key.host_resolver_source));
    entry_dict->SetKey(kNetworkIsolationKeyKey,
                       std::move(network_isolation_key_value));
    entry_dict->SetBoolean(kSecureKey, static_cast<bool>(key.secure));

    entry_list->Append(std::move(entry_dict));
  }
}

bool HostCache::RestoreFromListValue(const base::ListValue& old_cache) {
  // Reset the restore size to 0.
  restore_size_ = 0;

  for (auto it = old_cache.begin(); it != old_cache.end(); it++) {
    // If the cache is already full, don't bother prioritizing what to evict,
    // just stop restoring.
    if (size() == max_entries_)
      break;

    const base::DictionaryValue* entry_dict;
    if (!it->GetAsDictionary(&entry_dict))
      return false;

    std::string hostname;
    HostResolverFlags flags;
    std::string expiration;
    if (!entry_dict->GetString(kHostnameKey, &hostname) ||
        !entry_dict->GetInteger(kFlagsKey, &flags) ||
        !entry_dict->GetString(kExpirationKey, &expiration)) {
      return false;
    }

    // If there is no DnsQueryType, look for an AddressFamily.
    //
    // TODO(crbug.com/846423): Remove kAddressFamilyKey support after a enough
    // time has passed to minimize loss-of-persistence impact from backwards
    // incompatibility.
    int dns_query_type_val;
    DnsQueryType dns_query_type;
    if (entry_dict->GetInteger(kDnsQueryTypeKey, &dns_query_type_val)) {
      dns_query_type = static_cast<DnsQueryType>(dns_query_type_val);
    } else {
      int address_family;
      if (!entry_dict->GetInteger(kAddressFamilyKey, &address_family)) {
        return false;
      }
      dns_query_type = AddressFamilyToDnsQueryType(
          static_cast<AddressFamily>(address_family));
    }

    // HostResolverSource is optional.
    int host_resolver_source;
    if (!entry_dict->GetInteger(kHostResolverSourceKey,
                                &host_resolver_source)) {
      host_resolver_source = static_cast<int>(HostResolverSource::ANY);
    }

    const base::Value* network_isolation_key_value =
        entry_dict->FindKey(kNetworkIsolationKeyKey);
    NetworkIsolationKey network_isolation_key;
    if (!network_isolation_key_value ||
        !NetworkIsolationKey::FromValue(*network_isolation_key_value,
                                        &network_isolation_key)) {
      return false;
    }

    bool secure;
    if (!entry_dict->GetBoolean(kSecureKey, &secure)) {
      secure = false;
    }

    int error = OK;
    const base::ListValue* addresses_value = nullptr;
    const base::ListValue* text_records_value = nullptr;
    const base::ListValue* hostname_records_value = nullptr;
    const base::ListValue* host_ports_value = nullptr;
    const base::Value* esni_content_value = nullptr;
    if (!entry_dict->GetInteger(kErrorKey, &error)) {
      entry_dict->GetList(kAddressesKey, &addresses_value);
      entry_dict->GetList(kTextRecordsKey, &text_records_value);

      if (entry_dict->GetList(kHostnameResultsKey, &hostname_records_value) !=
          entry_dict->GetList(kHostPortsKey, &host_ports_value)) {
        return false;
      }

      entry_dict->Get(kEsniDataKey, &esni_content_value);
    }

    int64_t time_internal;
    if (!base::StringToInt64(expiration, &time_internal))
      return false;

    base::TimeTicks expiration_time =
        tick_clock_->NowTicks() -
        (base::Time::Now() - base::Time::FromInternalValue(time_internal));

    base::Optional<AddressList> address_list;
    if (!AddressListFromListValue(addresses_value, &address_list)) {
      return false;
    }

    base::Optional<std::vector<std::string>> text_records;
    if (text_records_value) {
      text_records.emplace();
      for (const base::Value& value : text_records_value->GetList()) {
        if (!value.is_string())
          return false;
        text_records.value().push_back(value.GetString());
      }
    }

    base::Optional<std::vector<HostPortPair>> hostname_records;
    if (hostname_records_value) {
      DCHECK(host_ports_value);
      if (hostname_records_value->GetList().size() !=
          host_ports_value->GetList().size()) {
        return false;
      }

      hostname_records.emplace();
      for (size_t i = 0; i < hostname_records_value->GetList().size(); ++i) {
        if (!hostname_records_value->GetList()[i].is_string() ||
            !host_ports_value->GetList()[i].is_int() ||
            !base::IsValueInRangeForNumericType<uint16_t>(
                host_ports_value->GetList()[i].GetInt())) {
          return false;
        }
        hostname_records.value().push_back(
            HostPortPair(hostname_records_value->GetList()[i].GetString(),
                         base::checked_cast<uint16_t>(
                             host_ports_value->GetList()[i].GetInt())));
      }
    }

    base::Optional<EsniContent> esni_content;
    if (esni_content_value &&
        !EsniContentFromValue(*esni_content_value, &esni_content)) {
      return false;
    }

    // Assume an empty address list if we have an address type and no results.
    if (IsAddressType(dns_query_type) && !address_list && !text_records &&
        !hostname_records && !esni_content) {
      address_list.emplace();
    }

    Key key(hostname, dns_query_type, flags,
            static_cast<HostResolverSource>(host_resolver_source),
            network_isolation_key);
    key.secure = secure;

    // If the key is already in the cache, assume it's more recent and don't
    // replace the entry.
    auto found = entries_.find(key);
    if (found == entries_.end()) {
      AddEntry(key, Entry(error, address_list, std::move(text_records),
                          std::move(hostname_records), std::move(esni_content),
                          Entry::SOURCE_UNKNOWN, expiration_time,
                          network_changes_ - 1));
      restore_size_++;
    }
  }
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

  entries_.erase(oldest_it);
}

const HostCache::Key* HostCache::GetMatchingKey(
    base::StringPiece hostname,
    HostCache::Entry::Source* source_out,
    HostCache::EntryStaleness* stale_out) {
  net::HostCache::Key cache_key;
  hostname.CopyToString(&cache_key.hostname);

  const std::pair<const HostCache::Key, HostCache::Entry>* cache_result =
      LookupStale(cache_key, tick_clock_->NowTicks(), stale_out,
                  true /* ignore_secure */);
  if (!cache_result && IsAddressType(cache_key.dns_query_type)) {
    // Might not have found the cache entry because the address_family or
    // host_resolver_flags in cache_key do not match those used for the
    // original DNS lookup. Try another common combination of address_family
    // and host_resolver_flags in an attempt to find a matching cache entry.
    cache_key.dns_query_type = DnsQueryType::A;
    cache_key.host_resolver_flags =
        net::HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6;
    cache_result = LookupStale(cache_key, tick_clock_->NowTicks(), stale_out,
                               true /* ignore_secure */);
    if (!cache_result)
      return nullptr;
  }

  if (source_out != nullptr)
    *source_out = cache_result->second.source();

  return &cache_result->first;
}

}  // namespace net

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/host_cache.h"

#include <algorithm>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <ostream>
#include <set>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/default_tick_clock.h"
#include "base/types/optional_util.h"
#include "base/value_iterators.h"
#include "net/base/address_family.h"
#include "net/base/ip_endpoint.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/trace_constants.h"
#include "net/base/tracing.h"
#include "net/dns/host_resolver.h"
#include "net/dns/host_resolver_internal_result.h"
#include "net/dns/https_record_rdata.h"
#include "net/dns/public/dns_protocol.h"
#include "net/dns/public/host_resolver_source.h"
#include "net/log/net_log.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/scheme_host_port.h"

namespace net {

namespace {

#define CACHE_HISTOGRAM_TIME(name, time) \
  UMA_HISTOGRAM_LONG_TIMES("DNS.HostCache." name, time)

#define CACHE_HISTOGRAM_COUNT(name, count) \
  UMA_HISTOGRAM_COUNTS_1000("DNS.HostCache." name, count)

#define CACHE_HISTOGRAM_ENUM(name, value, max) \
  UMA_HISTOGRAM_ENUMERATION("DNS.HostCache." name, value, max)

// String constants for dictionary keys.
const char kSchemeKey[] = "scheme";
const char kHostnameKey[] = "hostname";
const char kPortKey[] = "port";
const char kDnsQueryTypeKey[] = "dns_query_type";
const char kFlagsKey[] = "flags";
const char kHostResolverSourceKey[] = "host_resolver_source";
const char kSecureKey[] = "secure";
const char kNetworkAnonymizationKey[] = "network_anonymization_key";
const char kExpirationKey[] = "expiration";
const char kTtlKey[] = "ttl";
const char kPinnedKey[] = "pinned";
const char kNetworkChangesKey[] = "network_changes";
const char kNetErrorKey[] = "net_error";
const char kIpEndpointsKey[] = "ip_endpoints";
const char kEndpointAddressKey[] = "endpoint_address";
const char kEndpointPortKey[] = "endpoint_port";
const char kEndpointMetadatasKey[] = "endpoint_metadatas";
const char kEndpointMetadataWeightKey[] = "endpoint_metadata_weight";
const char kEndpointMetadataValueKey[] = "endpoint_metadata_value";
const char kAliasesKey[] = "aliases";
const char kAddressesKey[] = "addresses";
const char kTextRecordsKey[] = "text_records";
const char kHostnameResultsKey[] = "hostname_results";
const char kHostPortsKey[] = "host_ports";
const char kCanonicalNamesKey[] = "canonical_names";

base::Value IpEndpointToValue(const IPEndPoint& endpoint) {
  base::Value::Dict dictionary;
  dictionary.Set(kEndpointAddressKey, endpoint.ToStringWithoutPort());
  dictionary.Set(kEndpointPortKey, endpoint.port());
  return base::Value(std::move(dictionary));
}

std::optional<IPEndPoint> IpEndpointFromValue(const base::Value& value) {
  if (!value.is_dict())
    return std::nullopt;

  const base::Value::Dict& dict = value.GetDict();
  const std::string* ip_str = dict.FindString(kEndpointAddressKey);
  std::optional<int> port = dict.FindInt(kEndpointPortKey);

  if (!ip_str || !port ||
      !base::IsValueInRangeForNumericType<uint16_t>(port.value())) {
    return std::nullopt;
  }

  IPAddress ip;
  if (!ip.AssignFromIPLiteral(*ip_str))
    return std::nullopt;

  return IPEndPoint(ip, base::checked_cast<uint16_t>(port.value()));
}

base::Value EndpointMetadataPairToValue(
    const std::pair<HttpsRecordPriority, ConnectionEndpointMetadata>& pair) {
  base::Value::Dict dictionary;
  dictionary.Set(kEndpointMetadataWeightKey, pair.first);
  dictionary.Set(kEndpointMetadataValueKey, pair.second.ToValue());
  return base::Value(std::move(dictionary));
}

std::optional<std::pair<HttpsRecordPriority, ConnectionEndpointMetadata>>
EndpointMetadataPairFromValue(const base::Value& value) {
  if (!value.is_dict())
    return std::nullopt;

  const base::Value::Dict& dict = value.GetDict();
  std::optional<int> priority = dict.FindInt(kEndpointMetadataWeightKey);
  const base::Value* metadata_value = dict.Find(kEndpointMetadataValueKey);

  if (!priority || !base::IsValueInRangeForNumericType<HttpsRecordPriority>(
                       priority.value())) {
    return std::nullopt;
  }

  if (!metadata_value)
    return std::nullopt;
  std::optional<ConnectionEndpointMetadata> metadata =
      ConnectionEndpointMetadata::FromValue(*metadata_value);
  if (!metadata)
    return std::nullopt;

  return std::pair(base::checked_cast<HttpsRecordPriority>(priority.value()),
                   std::move(metadata).value());
}

bool IPEndPointsFromLegacyAddressListValue(
    const base::Value::List& value,
    std::vector<IPEndPoint>& ip_endpoints) {
  DCHECK(ip_endpoints.empty());
  for (const auto& it : value) {
    IPAddress address;
    const std::string* addr_string = it.GetIfString();
    if (!addr_string || !address.AssignFromIPLiteral(*addr_string)) {
      return false;
    }
    ip_endpoints.emplace_back(address, 0);
  }
  return true;
}

template <typename T>
void MergeLists(T& target, const T& source) {
  target.insert(target.end(), source.begin(), source.end());
}

template <typename T>
void MergeContainers(T& target, const T& source) {
  target.insert(source.begin(), source.end());
}

// Used to reject empty and IP literal (whether or not surrounded by brackets)
// hostnames.
bool IsValidHostname(std::string_view hostname) {
  if (hostname.empty())
    return false;

  IPAddress ip_address;
  if (ip_address.AssignFromIPLiteral(hostname) ||
      ParseURLHostnameToAddress(hostname, &ip_address)) {
    return false;
  }

  return true;
}

const std::string& GetHostname(
    const absl::variant<url::SchemeHostPort, std::string>& host) {
  const std::string* hostname;
  if (absl::holds_alternative<url::SchemeHostPort>(host)) {
    hostname = &absl::get<url::SchemeHostPort>(host).host();
  } else {
    DCHECK(absl::holds_alternative<std::string>(host));
    hostname = &absl::get<std::string>(host);
  }

  DCHECK(IsValidHostname(*hostname));
  return *hostname;
}

std::optional<DnsQueryType> GetDnsQueryType(int dns_query_type) {
  for (const auto& type : kDnsQueryTypes) {
    if (base::strict_cast<int>(type.first) == dns_query_type)
      return type.first;
  }
  return std::nullopt;
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

HostCache::Key::Key(absl::variant<url::SchemeHostPort, std::string> host,
                    DnsQueryType dns_query_type,
                    HostResolverFlags host_resolver_flags,
                    HostResolverSource host_resolver_source,
                    const NetworkAnonymizationKey& network_anonymization_key)
    : host(std::move(host)),
      dns_query_type(dns_query_type),
      host_resolver_flags(host_resolver_flags),
      host_resolver_source(host_resolver_source),
      network_anonymization_key(network_anonymization_key) {
  DCHECK(IsValidHostname(GetHostname(this->host)));
  if (absl::holds_alternative<url::SchemeHostPort>(this->host))
    DCHECK(absl::get<url::SchemeHostPort>(this->host).IsValid());
}

HostCache::Key::Key() = default;
HostCache::Key::Key(const Key& key) = default;
HostCache::Key::Key(Key&& key) = default;

HostCache::Key::~Key() = default;

HostCache::Entry::Entry(int error,
                        Source source,
                        std::optional<base::TimeDelta> ttl)
    : error_(error), source_(source), ttl_(ttl.value_or(kUnknownTtl)) {
  // If |ttl| has a value, must not be negative.
  DCHECK_GE(ttl.value_or(base::TimeDelta()), base::TimeDelta());
  DCHECK_NE(OK, error_);

  // host_cache.h defines its own `HttpsRecordPriority` due to
  // https_record_rdata.h not being allowed in the same places, but the types
  // should still be the same thing.
  static_assert(std::is_same<net::HttpsRecordPriority,
                             HostCache::Entry::HttpsRecordPriority>::value,
                "`net::HttpsRecordPriority` and "
                "`HostCache::Entry::HttpsRecordPriority` must be same type");
}

HostCache::Entry::Entry(
    const std::set<std::unique_ptr<HostResolverInternalResult>>& results,
    base::Time now,
    base::TimeTicks now_ticks,
    Source empty_source) {
  const HostResolverInternalResult* data_result = nullptr;
  const HostResolverInternalResult* metadata_result = nullptr;
  const HostResolverInternalResult* error_result = nullptr;
  std::vector<const HostResolverInternalResult*> alias_results;

  std::optional<base::TimeDelta> smallest_ttl =
      TtlFromInternalResults(results, now, now_ticks);
  std::optional<Source> source;
  for (auto it = results.cbegin(); it != results.cend();) {
    // Increment iterator now to allow extracting `result` (std::set::extract()
    // is guaranteed to not invalidate any iterators except those pointing to
    // the extracted value).
    const std::unique_ptr<HostResolverInternalResult>& result = *it++;

    Source result_source;
    switch (result->source()) {
      case HostResolverInternalResult::Source::kDns:
        result_source = SOURCE_DNS;
        break;
      case HostResolverInternalResult::Source::kHosts:
        result_source = SOURCE_HOSTS;
        break;
      case HostResolverInternalResult::Source::kUnknown:
        result_source = SOURCE_UNKNOWN;
        break;
    }

    switch (result->type()) {
      case HostResolverInternalResult::Type::kData:
        DCHECK(!data_result);  // Expect at most one data result.
        data_result = result.get();
        break;
      case HostResolverInternalResult::Type::kMetadata:
        DCHECK(!metadata_result);  // Expect at most one metadata result.
        metadata_result = result.get();
        break;
      case HostResolverInternalResult::Type::kError:
        DCHECK(!error_result);  // Expect at most one error result.
        error_result = result.get();
        break;
      case HostResolverInternalResult::Type::kAlias:
        alias_results.emplace_back(result.get());
        break;
    }

    // Expect all results to have the same source.
    DCHECK(!source.has_value() || source.value() == result_source);
    source = result_source;
  }

  ttl_ = smallest_ttl.value_or(kUnknownTtl);
  source_ = source.value_or(empty_source);

  if (error_result) {
    DCHECK(!data_result);
    DCHECK(!metadata_result);

    error_ = error_result->AsError().error();

    // For error results, should not create entry with a TTL unless it is a
    // cacheable error.
    if (!error_result->expiration().has_value() &&
        !error_result->timed_expiration().has_value()) {
      ttl_ = kUnknownTtl;
    }
  } else if (!data_result && !metadata_result) {
    // Only alias results (or completely empty results). Never cacheable due to
    // being equivalent to an error result without TTL.
    error_ = ERR_NAME_NOT_RESOLVED;
    ttl_ = kUnknownTtl;
  } else {
    error_ = OK;
  }

  if (data_result) {
    DCHECK(!error_result);
    DCHECK(!data_result->AsData().endpoints().empty() ||
           !data_result->AsData().strings().empty() ||
           !data_result->AsData().hosts().empty());
    // Data results should always be cacheable.
    DCHECK(data_result->expiration().has_value() ||
           data_result->timed_expiration().has_value());

    ip_endpoints_ = data_result->AsData().endpoints();
    text_records_ = data_result->AsData().strings();
    hostnames_ = data_result->AsData().hosts();
    canonical_names_ = {data_result->domain_name()};

    for (const auto* alias_result : alias_results) {
      aliases_.insert(alias_result->domain_name());
      aliases_.insert(alias_result->AsAlias().alias_target());
    }
    aliases_.insert(data_result->domain_name());
  }
  if (metadata_result) {
    DCHECK(!error_result);
    // Metadata results should always be cacheable.
    DCHECK(metadata_result->expiration().has_value() ||
           metadata_result->timed_expiration().has_value());

    endpoint_metadatas_ = metadata_result->AsMetadata().metadatas();

    // Even if otherwise empty, having the metadata result object signifies
    // receiving a compatible HTTPS record.
    https_record_compatibility_ = std::vector<bool>{true};

    if (endpoint_metadatas_.empty()) {
      error_ = ERR_NAME_NOT_RESOLVED;
    }
  }
}

HostCache::Entry::Entry(const Entry& entry) = default;

HostCache::Entry::Entry(Entry&& entry) = default;

HostCache::Entry::~Entry() = default;

std::vector<HostResolverEndpointResult> HostCache::Entry::GetEndpoints() const {
  std::vector<HostResolverEndpointResult> endpoints;

  if (ip_endpoints_.empty()) {
    return endpoints;
  }

  std::vector<ConnectionEndpointMetadata> metadatas = GetMetadatas();

  if (!metadatas.empty() && canonical_names_.size() == 1) {
    // Currently Chrome uses HTTPS records only when A and AAAA records are at
    // the same canonical name and that matches the HTTPS target name.
    for (ConnectionEndpointMetadata& metadata : metadatas) {
      if (!base::Contains(canonical_names_, metadata.target_name)) {
        continue;
      }
      endpoints.emplace_back();
      endpoints.back().ip_endpoints = ip_endpoints_;
      endpoints.back().metadata = std::move(metadata);
    }
  }

  // Add a final non-alternative endpoint at the end.
  endpoints.emplace_back();
  endpoints.back().ip_endpoints = ip_endpoints_;

  return endpoints;
}

std::vector<ConnectionEndpointMetadata> HostCache::Entry::GetMetadatas() const {
  std::vector<ConnectionEndpointMetadata> metadatas;
  HttpsRecordPriority last_priority = 0;
  for (const auto& metadata : endpoint_metadatas_) {
    // Ensure metadatas are iterated in priority order.
    DCHECK_GE(metadata.first, last_priority);
    last_priority = metadata.first;

    metadatas.push_back(metadata.second);
  }

  return metadatas;
}

std::optional<base::TimeDelta> HostCache::Entry::GetOptionalTtl() const {
  if (has_ttl())
    return ttl();
  else
    return std::nullopt;
}

// static
HostCache::Entry HostCache::Entry::MergeEntries(Entry front, Entry back) {
  // Only expected to merge OK or ERR_NAME_NOT_RESOLVED results.
  DCHECK(front.error() == OK || front.error() == ERR_NAME_NOT_RESOLVED);
  DCHECK(back.error() == OK || back.error() == ERR_NAME_NOT_RESOLVED);

  // Build results in |front| to preserve unmerged fields.

  front.error_ =
      front.error() == OK || back.error() == OK ? OK : ERR_NAME_NOT_RESOLVED;

  MergeLists(front.ip_endpoints_, back.ip_endpoints_);
  MergeContainers(front.endpoint_metadatas_, back.endpoint_metadatas_);
  MergeContainers(front.aliases_, back.aliases_);
  MergeLists(front.text_records_, back.text_records());
  MergeLists(front.hostnames_, back.hostnames());
  MergeLists(front.https_record_compatibility_,
             back.https_record_compatibility_);
  MergeContainers(front.canonical_names_, back.canonical_names_);

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

  for (IPEndPoint& endpoint : copy.ip_endpoints_) {
    if (endpoint.port() == 0) {
      endpoint = IPEndPoint(endpoint.address(), port);
    }
  }

  for (HostPortPair& hostname : copy.hostnames_) {
    if (hostname.port() == 0) {
      hostname = HostPortPair(hostname.host(), port);
    }
  }

  return copy;
}

HostCache::Entry& HostCache::Entry::operator=(const Entry& entry) = default;

HostCache::Entry& HostCache::Entry::operator=(Entry&& entry) = default;

HostCache::Entry::Entry(int error,
                        std::vector<IPEndPoint> ip_endpoints,
                        std::set<std::string> aliases,
                        Source source,
                        std::optional<base::TimeDelta> ttl)
    : error_(error),
      ip_endpoints_(std::move(ip_endpoints)),
      aliases_(std::move(aliases)),
      source_(source),
      ttl_(ttl ? ttl.value() : kUnknownTtl) {
  DCHECK(!ttl || ttl.value() >= base::TimeDelta());
}

HostCache::Entry::Entry(const HostCache::Entry& entry,
                        base::TimeTicks now,
                        base::TimeDelta ttl,
                        int network_changes)
    : error_(entry.error()),
      ip_endpoints_(entry.ip_endpoints_),
      endpoint_metadatas_(entry.endpoint_metadatas_),
      aliases_(entry.aliases()),
      text_records_(entry.text_records()),
      hostnames_(entry.hostnames()),
      https_record_compatibility_(entry.https_record_compatibility_),
      source_(entry.source()),
      pinning_(entry.pinning()),
      canonical_names_(entry.canonical_names()),
      ttl_(entry.ttl()),
      expires_(now + ttl),
      network_changes_(network_changes) {}

HostCache::Entry::Entry(
    int error,
    std::vector<IPEndPoint> ip_endpoints,
    std::multimap<HttpsRecordPriority, ConnectionEndpointMetadata>
        endpoint_metadatas,
    std::set<std::string> aliases,
    std::vector<std::string>&& text_records,
    std::vector<HostPortPair>&& hostnames,
    std::vector<bool>&& https_record_compatibility,
    Source source,
    base::TimeTicks expires,
    int network_changes)
    : error_(error),
      ip_endpoints_(std::move(ip_endpoints)),
      endpoint_metadatas_(std::move(endpoint_metadatas)),
      aliases_(std::move(aliases)),
      text_records_(std::move(text_records)),
      hostnames_(std::move(hostnames)),
      https_record_compatibility_(std::move(https_record_compatibility)),
      source_(source),
      expires_(expires),
      network_changes_(network_changes) {}

void HostCache::Entry::PrepareForCacheInsertion() {
  https_record_compatibility_.clear();
}

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
  return base::Value(GetAsValue(false /* include_staleness */));
}

base::Value::Dict HostCache::Entry::GetAsValue(bool include_staleness) const {
  base::Value::Dict entry_dict;

  if (include_staleness) {
    // The kExpirationKey value is using TimeTicks instead of Time used if
    // |include_staleness| is false, so it cannot be used to deserialize.
    // This is ok as it is used only for netlog.
    entry_dict.Set(kExpirationKey, NetLog::TickCountToString(expires()));
    entry_dict.Set(kTtlKey, base::saturated_cast<int>(ttl().InMilliseconds()));
    entry_dict.Set(kNetworkChangesKey, network_changes());
    // The "pinned" status is meaningful only if "network_changes" is also
    // preserved.
    if (pinning())
      entry_dict.Set(kPinnedKey, *pinning());
  } else {
    // Convert expiration time in TimeTicks to Time for serialization, using a
    // string because base::Value doesn't handle 64-bit integers.
    base::Time expiration_time =
        base::Time::Now() - (base::TimeTicks::Now() - expires());
    entry_dict.Set(kExpirationKey,
                   base::NumberToString(expiration_time.ToInternalValue()));
  }

  if (error() != OK) {
    entry_dict.Set(kNetErrorKey, error());
  } else {
    base::Value::List ip_endpoints_list;
    for (const IPEndPoint& ip_endpoint : ip_endpoints_) {
      ip_endpoints_list.Append(IpEndpointToValue(ip_endpoint));
    }
    entry_dict.Set(kIpEndpointsKey, std::move(ip_endpoints_list));

    base::Value::List endpoint_metadatas_list;
    for (const auto& endpoint_metadata_pair : endpoint_metadatas_) {
      endpoint_metadatas_list.Append(
          EndpointMetadataPairToValue(endpoint_metadata_pair));
    }
    entry_dict.Set(kEndpointMetadatasKey, std::move(endpoint_metadatas_list));

    base::Value::List alias_list;
    for (const std::string& alias : aliases()) {
      alias_list.Append(alias);
    }
    entry_dict.Set(kAliasesKey, std::move(alias_list));

    // Append all resolved text records.
    base::Value::List text_list_value;
    for (const std::string& text_record : text_records()) {
      text_list_value.Append(text_record);
    }
    entry_dict.Set(kTextRecordsKey, std::move(text_list_value));

    // Append all the resolved hostnames.
    base::Value::List hostnames_value;
    base::Value::List host_ports_value;
    for (const HostPortPair& hostname : hostnames()) {
      hostnames_value.Append(hostname.host());
      host_ports_value.Append(hostname.port());
    }
    entry_dict.Set(kHostnameResultsKey, std::move(hostnames_value));
    entry_dict.Set(kHostPortsKey, std::move(host_ports_value));

    base::Value::List canonical_names_list;
    for (const std::string& canonical_name : canonical_names()) {
      canonical_names_list.Append(canonical_name);
    }
    entry_dict.Set(kCanonicalNamesKey, std::move(canonical_names_list));
  }

  return entry_dict;
}

// static
std::optional<base::TimeDelta> HostCache::Entry::TtlFromInternalResults(
    const std::set<std::unique_ptr<HostResolverInternalResult>>& results,
    base::Time now,
    base::TimeTicks now_ticks) {
  std::optional<base::TimeDelta> smallest_ttl;
  for (const std::unique_ptr<HostResolverInternalResult>& result : results) {
    if (result->expiration().has_value()) {
      smallest_ttl = std::min(smallest_ttl.value_or(base::TimeDelta::Max()),
                              result->expiration().value() - now_ticks);
    }
    if (result->timed_expiration().has_value()) {
      smallest_ttl = std::min(smallest_ttl.value_or(base::TimeDelta::Max()),
                              result->timed_expiration().value() - now);
    }
  }
  return smallest_ttl;
}

// static
const HostCache::EntryStaleness HostCache::kNotStale = {base::Seconds(-1), 0,
                                                        0};

HostCache::HostCache(size_t max_entries)
    : max_entries_(max_entries),
      tick_clock_(base::DefaultTickClock::GetInstance()) {}

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
    if (staleness1.expired_by.is_negative() &&
        staleness2.expired_by >= base::TimeDelta()) {
      return result1;
    }
    if (staleness1.expired_by >= base::TimeDelta() &&
        staleness2.expired_by.is_negative()) {
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

  bool has_active_pin = false;
  bool result_changed = false;
  auto it = entries_.find(key);
  if (it != entries_.end()) {
    has_active_pin = HasActivePin(it->second);

    // TODO(juliatuttle): Remember some old metadata (hit count or frequency or
    // something like that) if it's useful for better eviction algorithms?
    result_changed = entry.error() == OK && !it->second.ContentsEqual(entry);
    entries_.erase(it);
  } else {
    result_changed = true;
    // This loop almost always runs at most once, for total runtime
    // O(max_entries_).  It only runs more than once if the cache was over-full
    // due to pinned entries, and this is the first call to Set() after
    // Invalidate().  The amortized cost remains O(size()) per call to Set().
    while (size() >= max_entries_ && EvictOneEntry(now)) {
    }
  }

  Entry entry_for_cache(entry, now, ttl, network_changes_);
  entry_for_cache.set_pinning(entry.pinning().value_or(has_active_pin));
  entry_for_cache.PrepareForCacheInsertion();
  AddEntry(key, std::move(entry_for_cache));

  if (delegate_ && result_changed)
    delegate_->ScheduleWrite();
}

const HostCache::Key* HostCache::GetMatchingKeyForTesting(
    std::string_view hostname,
    HostCache::Entry::Source* source_out,
    HostCache::EntryStaleness* stale_out) const {
  for (const EntryMap::value_type& entry : entries_) {
    if (GetHostname(entry.first.host) == hostname) {
      if (source_out != nullptr)
        *source_out = entry.second.source();
      if (stale_out != nullptr) {
        entry.second.GetStaleness(tick_clock_->NowTicks(), network_changes_,
                                  stale_out);
      }
      return &entry.first;
    }
  }

  return nullptr;
}

void HostCache::AddEntry(const Key& key, Entry&& entry) {
  DCHECK_EQ(0u, entries_.count(key));
  DCHECK(entry.pinning().has_value());
  entries_.emplace(key, std::move(entry));
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
    const base::RepeatingCallback<bool(const std::string&)>& host_filter) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (host_filter.is_null()) {
    clear();
    return;
  }

  bool changed = false;
  for (auto it = entries_.begin(); it != entries_.end();) {
    auto next_it = std::next(it);

    if (host_filter.Run(GetHostname(it->first.host))) {
      entries_.erase(it);
      changed = true;
    }

    it = next_it;
  }

  if (delegate_ && changed)
    delegate_->ScheduleWrite();
}

void HostCache::GetList(base::Value::List& entry_list,
                        bool include_staleness,
                        SerializationType serialization_type) const {
  entry_list.clear();

  for (const auto& pair : entries_) {
    const Key& key = pair.first;
    const Entry& entry = pair.second;

    base::Value network_anonymization_key_value;
    if (serialization_type == SerializationType::kRestorable) {
      // Don't save entries associated with ephemeral NetworkAnonymizationKeys.
      if (!key.network_anonymization_key.ToValue(
              &network_anonymization_key_value)) {
        continue;
      }
    } else {
      // ToValue() fails for transient NAKs, since they should never be
      // serialized to disk in a restorable format, so use ToDebugString() when
      // serializing for debugging instead of for restoring from disk.
      network_anonymization_key_value =
          base::Value(key.network_anonymization_key.ToDebugString());
    }

    base::Value::Dict entry_dict = entry.GetAsValue(include_staleness);

    const auto* host = absl::get_if<url::SchemeHostPort>(&key.host);
    if (host) {
      entry_dict.Set(kSchemeKey, host->scheme());
      entry_dict.Set(kHostnameKey, host->host());
      entry_dict.Set(kPortKey, host->port());
    } else {
      entry_dict.Set(kHostnameKey, absl::get<std::string>(key.host));
    }

    entry_dict.Set(kDnsQueryTypeKey,
                   base::strict_cast<int>(key.dns_query_type));
    entry_dict.Set(kFlagsKey, key.host_resolver_flags);
    entry_dict.Set(kHostResolverSourceKey,
                   base::strict_cast<int>(key.host_resolver_source));
    entry_dict.Set(kNetworkAnonymizationKey,
                   std::move(network_anonymization_key_value));
    entry_dict.Set(kSecureKey, key.secure);

    entry_list.Append(std::move(entry_dict));
  }
}

bool HostCache::RestoreFromListValue(const base::Value::List& old_cache) {
  // Reset the restore size to 0.
  restore_size_ = 0;

  for (const auto& entry : old_cache) {
    // If the cache is already full, don't bother prioritizing what to evict,
    // just stop restoring.
    if (size() == max_entries_)
      break;

    if (!entry.is_dict())
      return false;

    const base::Value::Dict& entry_dict = entry.GetDict();
    const std::string* hostname_ptr = entry_dict.FindString(kHostnameKey);
    if (!hostname_ptr || !IsValidHostname(*hostname_ptr)) {
      return false;
    }

    // Use presence of scheme to determine host type.
    const std::string* scheme_ptr = entry_dict.FindString(kSchemeKey);
    absl::variant<url::SchemeHostPort, std::string> host;
    if (scheme_ptr) {
      std::optional<int> port = entry_dict.FindInt(kPortKey);
      if (!port || !base::IsValueInRangeForNumericType<uint16_t>(port.value()))
        return false;

      url::SchemeHostPort scheme_host_port(*scheme_ptr, *hostname_ptr,
                                           port.value());
      if (!scheme_host_port.IsValid())
        return false;
      host = std::move(scheme_host_port);
    } else {
      host = *hostname_ptr;
    }

    const std::string* expiration_ptr = entry_dict.FindString(kExpirationKey);
    std::optional<int> maybe_flags = entry_dict.FindInt(kFlagsKey);
    if (expiration_ptr == nullptr || !maybe_flags.has_value())
      return false;
    std::string expiration(*expiration_ptr);
    HostResolverFlags flags = maybe_flags.value();

    std::optional<int> maybe_dns_query_type =
        entry_dict.FindInt(kDnsQueryTypeKey);
    if (!maybe_dns_query_type.has_value())
      return false;
    std::optional<DnsQueryType> dns_query_type =
        GetDnsQueryType(maybe_dns_query_type.value());
    if (!dns_query_type.has_value())
      return false;
    // HostResolverSource is optional.
    int host_resolver_source =
        entry_dict.FindInt(kHostResolverSourceKey)
            .value_or(base::strict_cast<int>(HostResolverSource::ANY));

    const base::Value* network_anonymization_key_value =
        entry_dict.Find(kNetworkAnonymizationKey);
    NetworkAnonymizationKey network_anonymization_key;
    if (!network_anonymization_key_value ||
        network_anonymization_key_value->type() == base::Value::Type::STRING ||
        !NetworkAnonymizationKey::FromValue(*network_anonymization_key_value,
                                            &network_anonymization_key)) {
      return false;
    }

    bool secure = entry_dict.FindBool(kSecureKey).value_or(false);

    int error = OK;
    const base::Value::List* ip_endpoints_list = nullptr;
    const base::Value::List* endpoint_metadatas_list = nullptr;
    const base::Value::List* aliases_list = nullptr;
    const base::Value::List* legacy_addresses_list = nullptr;
    const base::Value::List* text_records_list = nullptr;
    const base::Value::List* hostname_records_list = nullptr;
    const base::Value::List* host_ports_list = nullptr;
    const base::Value::List* canonical_names_list = nullptr;
    std::optional<int> maybe_error = entry_dict.FindInt(kNetErrorKey);
    std::optional<bool> maybe_pinned = entry_dict.FindBool(kPinnedKey);
    if (maybe_error.has_value()) {
      error = maybe_error.value();
    } else {
      ip_endpoints_list = entry_dict.FindList(kIpEndpointsKey);
      endpoint_metadatas_list = entry_dict.FindList(kEndpointMetadatasKey);
      aliases_list = entry_dict.FindList(kAliasesKey);
      legacy_addresses_list = entry_dict.FindList(kAddressesKey);
      text_records_list = entry_dict.FindList(kTextRecordsKey);
      hostname_records_list = entry_dict.FindList(kHostnameResultsKey);
      host_ports_list = entry_dict.FindList(kHostPortsKey);
      canonical_names_list = entry_dict.FindList(kCanonicalNamesKey);

      if ((hostname_records_list == nullptr && host_ports_list != nullptr) ||
          (hostname_records_list != nullptr && host_ports_list == nullptr)) {
        return false;
      }
    }

    int64_t time_internal;
    if (!base::StringToInt64(expiration, &time_internal))
      return false;

    base::TimeTicks expiration_time =
        tick_clock_->NowTicks() -
        (base::Time::Now() - base::Time::FromInternalValue(time_internal));

    std::vector<IPEndPoint> ip_endpoints;
    if (ip_endpoints_list) {
      for (const base::Value& ip_endpoint_value : *ip_endpoints_list) {
        std::optional<IPEndPoint> ip_endpoint =
            IpEndpointFromValue(ip_endpoint_value);
        if (!ip_endpoint)
          return false;
        ip_endpoints.push_back(std::move(ip_endpoint).value());
      }
    }

    std::multimap<HttpsRecordPriority, ConnectionEndpointMetadata>
        endpoint_metadatas;
    if (endpoint_metadatas_list) {
      for (const base::Value& endpoint_metadata_value :
           *endpoint_metadatas_list) {
        std::optional<
            std::pair<HttpsRecordPriority, ConnectionEndpointMetadata>>
            pair = EndpointMetadataPairFromValue(endpoint_metadata_value);
        if (!pair)
          return false;
        endpoint_metadatas.insert(std::move(pair).value());
      }
    }

    std::set<std::string> aliases;
    if (aliases_list) {
      for (const base::Value& alias_value : *aliases_list) {
        if (!alias_value.is_string())
          return false;
        aliases.insert(alias_value.GetString());
      }
    }

    // `addresses` field was supported until M105. We keep reading this field
    // for backward compatibility for several milestones.
    if (legacy_addresses_list) {
      if (!ip_endpoints.empty()) {
        return false;
      }
      if (!IPEndPointsFromLegacyAddressListValue(*legacy_addresses_list,
                                                 ip_endpoints)) {
        return false;
      }
    }

    std::vector<std::string> text_records;
    if (text_records_list) {
      for (const base::Value& value : *text_records_list) {
        if (!value.is_string())
          return false;
        text_records.push_back(value.GetString());
      }
    }

    std::vector<HostPortPair> hostname_records;
    if (hostname_records_list) {
      DCHECK(host_ports_list);
      if (hostname_records_list->size() != host_ports_list->size()) {
        return false;
      }

      for (size_t i = 0; i < hostname_records_list->size(); ++i) {
        if (!(*hostname_records_list)[i].is_string() ||
            !(*host_ports_list)[i].is_int() ||
            !base::IsValueInRangeForNumericType<uint16_t>(
                (*host_ports_list)[i].GetInt())) {
          return false;
        }
        hostname_records.emplace_back(
            (*hostname_records_list)[i].GetString(),
            base::checked_cast<uint16_t>((*host_ports_list)[i].GetInt()));
      }
    }

    std::set<std::string> canonical_names;
    if (canonical_names_list) {
      for (const auto& item : *canonical_names_list) {
        const std::string* name = item.GetIfString();
        if (!name)
          return false;
        canonical_names.insert(*name);
      }
    }

    // We do not intend to serialize experimental results with the host cache.
    std::vector<bool> experimental_results;

    Key key(std::move(host), dns_query_type.value(), flags,
            static_cast<HostResolverSource>(host_resolver_source),
            network_anonymization_key);
    key.secure = secure;

    // If the key is already in the cache, assume it's more recent and don't
    // replace the entry.
    auto found = entries_.find(key);
    if (found == entries_.end()) {
      Entry new_entry(error, std::move(ip_endpoints),
                      std::move(endpoint_metadatas), std::move(aliases),
                      std::move(text_records), std::move(hostname_records),
                      std::move(experimental_results), Entry::SOURCE_UNKNOWN,
                      expiration_time, network_changes_ - 1);
      new_entry.set_pinning(maybe_pinned.value_or(false));
      new_entry.set_canonical_names(std::move(canonical_names));
      AddEntry(key, std::move(new_entry));
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

bool HostCache::EvictOneEntry(base::TimeTicks now) {
  DCHECK_LT(0u, entries_.size());

  std::optional<net::HostCache::EntryMap::iterator> oldest_it;
  for (auto it = entries_.begin(); it != entries_.end(); ++it) {
    const Entry& entry = it->second;
    if (HasActivePin(entry)) {
      continue;
    }

    if (!oldest_it) {
      oldest_it = it;
      continue;
    }

    const Entry& oldest = (*oldest_it)->second;
    if ((entry.expires() < oldest.expires()) &&
        (entry.IsStale(now, network_changes_) ||
         !oldest.IsStale(now, network_changes_))) {
      oldest_it = it;
    }
  }

  if (oldest_it) {
    entries_.erase(*oldest_it);
    return true;
  }
  return false;
}

bool HostCache::HasActivePin(const Entry& entry) {
  return entry.pinning().value_or(false) &&
         entry.network_changes() == network_changes();
}

}  // namespace net

// Debug logging support
std::ostream& operator<<(std::ostream& out,
                         const net::HostCache::EntryStaleness& s) {
  return out << "EntryStaleness{" << s.expired_by << ", " << s.network_changes
             << ", " << s.stale_hits << "}";
}

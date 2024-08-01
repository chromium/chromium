// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_util.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>

#include <cstring>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/byte_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "net/dns/public/dns_protocol.h"
#include "net/dns/public/doh_provider_entry.h"
#include "net/dns/public/util.h"
#include "net/third_party/uri_template/uri_template.h"

#if BUILDFLAG(IS_POSIX)
#include <net/if.h>
#include <netinet/in.h>
#if !BUILDFLAG(IS_ANDROID)
#include <ifaddrs.h>
#endif  // !BUILDFLAG(IS_ANDROID)
#endif  // BUILDFLAG(IS_POSIX)

#if BUILDFLAG(IS_ANDROID)
#include "net/android/network_library.h"
#endif

namespace net {
namespace {

DohProviderEntry::List GetDohProviderEntriesFromNameservers(
    const std::vector<IPEndPoint>& dns_servers) {
  const DohProviderEntry::List& providers = DohProviderEntry::GetList();
  DohProviderEntry::List entries;

  for (const auto& server : dns_servers) {
    for (const net::DohProviderEntry* entry : providers) {
      // DoH servers should only be added once.
      // Note: Check whether the provider is enabled *after* we've determined
      // that the IP addresses match so that if we are doing experimentation via
      // Finch, the experiment only includes possible users of the
      // corresponding DoH provider (since the client will be included in the
      // experiment if the provider feature flag is checked).
      if (base::Contains(entry->ip_addresses, server.address()) &&
          base::FeatureList::IsEnabled(entry->feature.get()) &&
          !base::Contains(entries, entry)) {
        entries.push_back(entry);
      }
    }
  }
  return entries;
}

}  // namespace

std::string GetURLFromTemplateWithoutParameters(const string& server_template) {
  std::string url_string;
  std::unordered_map<string, string> parameters;
  uri_template::Expand(server_template, parameters, &url_string);
  return url_string;
}

namespace {

bool GetTimeDeltaForConnectionTypeFromFieldTrial(
    const char* field_trial,
    NetworkChangeNotifier::ConnectionType type,
    base::TimeDelta* out) {
  std::string group = base::FieldTrialList::FindFullName(field_trial);
  if (group.empty())
    return false;
  std::vector<std::string_view> group_parts = base::SplitStringPiece(
      group, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (type < 0)
    return false;
  size_t type_size = static_cast<size_t>(type);
  if (type_size >= group_parts.size())
    return false;
  int64_t ms;
  if (!base::StringToInt64(group_parts[type_size], &ms))
    return false;
  *out = base::Milliseconds(ms);
  return true;
}

}  // namespace

base::TimeDelta GetTimeDeltaForConnectionTypeFromFieldTrialOrDefault(
    const char* field_trial,
    base::TimeDelta default_delta,
    NetworkChangeNotifier::ConnectionType type) {
  base::TimeDelta out;
  if (!GetTimeDeltaForConnectionTypeFromFieldTrial(field_trial, type, &out))
    out = default_delta;
  return out;
}

std::string CreateNamePointer(uint16_t offset) {
  DCHECK_EQ(offset & ~dns_protocol::kOffsetMask, 0);
  std::array<uint8_t, 2> buf = base::U16ToBigEndian(offset);
  buf[0u] |= dns_protocol::kLabelPointer;
  return std::string(buf.begin(), buf.end());
}

uint16_t DnsQueryTypeToQtype(DnsQueryType dns_query_type) {
  switch (dns_query_type) {
    case DnsQueryType::UNSPECIFIED:
      NOTREACHED_IN_MIGRATION();
      return 0;
    case DnsQueryType::A:
      return dns_protocol::kTypeA;
    case DnsQueryType::AAAA:
      return dns_protocol::kTypeAAAA;
    case DnsQueryType::TXT:
      return dns_protocol::kTypeTXT;
    case DnsQueryType::PTR:
      return dns_protocol::kTypePTR;
    case DnsQueryType::SRV:
      return dns_protocol::kTypeSRV;
    case DnsQueryType::HTTPS:
      return dns_protocol::kTypeHttps;
  }
}

DnsQueryType AddressFamilyToDnsQueryType(AddressFamily address_family) {
  switch (address_family) {
    case ADDRESS_FAMILY_UNSPECIFIED:
      return DnsQueryType::UNSPECIFIED;
    case ADDRESS_FAMILY_IPV4:
      return DnsQueryType::A;
    case ADDRESS_FAMILY_IPV6:
      return DnsQueryType::AAAA;
    default:
      NOTREACHED_IN_MIGRATION();
      return DnsQueryType::UNSPECIFIED;
  }
}

std::vector<DnsOverHttpsServerConfig> GetDohUpgradeServersFromDotHostname(
    const std::string& dot_server) {
  std::vector<DnsOverHttpsServerConfig> doh_servers;

  if (dot_server.empty())
    return doh_servers;

  for (const net::DohProviderEntry* entry : DohProviderEntry::GetList()) {
    // Note: Check whether the provider is enabled *after* we've determined that
    // the hostnames match so that if we are doing experimentation via Finch,
    // the experiment only includes possible users of the corresponding DoH
    // provider (since the client will be included in the experiment if the
    // provider feature flag is checked).
    if (base::Contains(entry->dns_over_tls_hostnames, dot_server) &&
        base::FeatureList::IsEnabled(entry->feature.get())) {
      doh_servers.push_back(entry->doh_server_config);
    }
  }
  return doh_servers;
}

std::vector<DnsOverHttpsServerConfig> GetDohUpgradeServersFromNameservers(
    const std::vector<IPEndPoint>& dns_servers) {
  const auto entries = GetDohProviderEntriesFromNameservers(dns_servers);
  std::vector<DnsOverHttpsServerConfig> doh_servers;
  doh_servers.reserve(entries.size());
  base::ranges::transform(entries, std::back_inserter(doh_servers),
                          &DohProviderEntry::doh_server_config);
  return doh_servers;
}

std::string GetDohProviderIdForHistogramFromServerConfig(
    const DnsOverHttpsServerConfig& doh_server) {
  const auto& entries = DohProviderEntry::GetList();
  const auto it = base::ranges::find(entries, doh_server,
                                     &DohProviderEntry::doh_server_config);
  return it != entries.end() ? (*it)->provider : "Other";
}

std::string GetDohProviderIdForHistogramFromNameserver(
    const IPEndPoint& nameserver) {
  const auto entries = GetDohProviderEntriesFromNameservers({nameserver});
  return entries.empty() ? "Other" : entries[0]->provider;
}

std::string SecureDnsModeToString(const SecureDnsMode secure_dns_mode) {
  switch (secure_dns_mode) {
    case SecureDnsMode::kOff:
      return "Off";
    case SecureDnsMode::kAutomatic:
      return "Automatic";
    case SecureDnsMode::kSecure:
      return "Secure";
  }
}

}  // namespace net

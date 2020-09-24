// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_util.h"

#include <errno.h>
#include <limits.h>

#include <cstring>
#include <unordered_map>
#include <vector>

#include "base/big_endian.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "net/base/address_list.h"
#include "net/base/url_util.h"
#include "net/dns/public/dns_protocol.h"
#include "net/dns/public/doh_provider_entry.h"
#include "net/dns/public/util.h"
#include "net/third_party/uri_template/uri_template.h"
#include "url/url_canon.h"

namespace {

// RFC 1035, section 2.3.4: labels 63 octets or less.
// Section 3.1: Each label is represented as a one octet length field followed
// by that number of octets.
const int kMaxLabelLength = 63;

// RFC 1035, section 4.1.4: the first two bits of a 16-bit name pointer are
// ones.
const uint16_t kFlagNamePointer = 0xc000;

}  // namespace

#if defined(OS_POSIX)
#include <netinet/in.h>
#if !defined(OS_NACL)
#include <net/if.h>
#if !defined(OS_ANDROID)
#include <ifaddrs.h>
#endif  // !defined(OS_ANDROID)
#endif  // !defined(OS_NACL)
#endif  // defined(OS_POSIX)

#if defined(OS_ANDROID)
#include "net/android/network_library.h"
#endif

namespace net {
namespace {

// Based on DJB's public domain code.
bool DNSDomainFromDot(const base::StringPiece& dotted,
                      bool is_unrestricted,
                      std::string* out) {
  const char* buf = dotted.data();
  size_t n = dotted.size();
  char label[kMaxLabelLength];
  size_t labellen = 0; /* <= sizeof label */
  char name[dns_protocol::kMaxNameLength];
  size_t namelen = 0; /* <= sizeof name */
  char ch;

  for (;;) {
    if (!n)
      break;
    ch = *buf++;
    --n;
    if (ch == '.') {
      // Don't allow empty labels per http://crbug.com/456391.
      if (!labellen)
        return false;
      if (namelen + labellen + 1 > sizeof name)
        return false;
      name[namelen++] = static_cast<char>(labellen);
      memcpy(name + namelen, label, labellen);
      namelen += labellen;
      labellen = 0;
      continue;
    }
    if (labellen >= sizeof label)
      return false;
    if (!is_unrestricted && !IsValidHostLabelCharacter(ch, labellen == 0)) {
      return false;
    }
    label[labellen++] = ch;
  }

  // Allow empty label at end of name to disable suffix search.
  if (labellen) {
    if (namelen + labellen + 1 > sizeof name)
      return false;
    name[namelen++] = static_cast<char>(labellen);
    memcpy(name + namelen, label, labellen);
    namelen += labellen;
    labellen = 0;
  }

  if (namelen + 1 > sizeof name)
    return false;
  if (namelen == 0)  // Empty names e.g. "", "." are not valid.
    return false;
  name[namelen++] = 0;  // This is the root label (of length 0).

  *out = std::string(name, namelen);
  return true;
}

DohProviderEntry::List GetDohProviderEntriesFromNameservers(
    const std::vector<IPEndPoint>& dns_servers,
    const std::vector<std::string>& excluded_providers) {
  const DohProviderEntry::List& providers = DohProviderEntry::GetList();
  DohProviderEntry::List entries;

  for (const auto& server : dns_servers) {
    for (const auto* entry : providers) {
      if (base::Contains(excluded_providers, entry->provider))
        continue;

      // DoH servers should only be added once.
      if (base::Contains(entry->ip_addresses, server.address()) &&
          !base::Contains(entries, entry)) {
        entries.push_back(entry);
      }
    }
  }
  return entries;
}

}  // namespace

bool DNSDomainFromDot(const base::StringPiece& dotted, std::string* out) {
  return DNSDomainFromDot(dotted, false /* is_unrestricted */, out);
}

bool DNSDomainFromUnrestrictedDot(const base::StringPiece& dotted,
                                  std::string* out) {
  return DNSDomainFromDot(dotted, true /* is_unrestricted */, out);
}

bool IsValidDNSDomain(const base::StringPiece& dotted) {
  std::string dns_formatted;
  return DNSDomainFromDot(dotted, &dns_formatted);
}

bool IsValidUnrestrictedDNSDomain(const base::StringPiece& dotted) {
  std::string dns_formatted;
  return DNSDomainFromUnrestrictedDot(dotted, &dns_formatted);
}

bool IsValidHostLabelCharacter(char c, bool is_first_char) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9') || (!is_first_char && c == '-') || c == '_';
}

std::string DNSDomainToString(const base::StringPiece& domain) {
  std::string ret;

  for (unsigned i = 0; i < domain.size() && domain[i]; i += domain[i] + 1) {
#if CHAR_MIN < 0
    if (domain[i] < 0)
      return std::string();
#endif
    if (domain[i] > kMaxLabelLength)
      return std::string();

    if (i)
      ret += ".";

    if (static_cast<unsigned>(domain[i]) + i + 1 > domain.size())
      return std::string();

    ret.append(domain.data() + i + 1, domain[i]);
  }
  return ret;
}

std::string GetURLFromTemplateWithoutParameters(const string& server_template) {
  std::string url_string;
  std::unordered_map<string, string> parameters;
  uri_template::Expand(server_template, parameters, &url_string);
  return url_string;
}

#if !defined(OS_NACL)
namespace {

bool GetTimeDeltaForConnectionTypeFromFieldTrial(
    const char* field_trial,
    NetworkChangeNotifier::ConnectionType type,
    base::TimeDelta* out) {
  std::string group = base::FieldTrialList::FindFullName(field_trial);
  if (group.empty())
    return false;
  std::vector<base::StringPiece> group_parts = base::SplitStringPiece(
      group, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (type < 0)
    return false;
  size_t type_size = static_cast<size_t>(type);
  if (type_size >= group_parts.size())
    return false;
  int64_t ms;
  if (!base::StringToInt64(group_parts[type_size], &ms))
    return false;
  *out = base::TimeDelta::FromMilliseconds(ms);
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
#endif  // !defined(OS_NACL)

AddressListDeltaType FindAddressListDeltaType(const AddressList& a,
                                              const AddressList& b) {
  bool pairwise_mismatch = false;
  bool any_match = false;
  bool any_missing = false;
  bool same_size = a.size() == b.size();

  for (size_t i = 0; i < a.size(); ++i) {
    bool this_match = false;
    for (size_t j = 0; j < b.size(); ++j) {
      if (a[i] == b[j]) {
        any_match = true;
        this_match = true;
        // If there is no match before, and the current match, this means
        // DELTA_OVERLAP.
        if (any_missing)
          return DELTA_OVERLAP;
      } else if (i == j) {
        pairwise_mismatch = true;
      }
    }
    if (!this_match) {
      any_missing = true;
      // If any match has occurred before, then there is no need to compare the
      // remaining addresses. This means DELTA_OVERLAP.
      if (any_match)
        return DELTA_OVERLAP;
    }
  }

  if (same_size && !pairwise_mismatch)
    return DELTA_IDENTICAL;
  else if (same_size && !any_missing)
    return DELTA_REORDERED;
  else if (any_match)
    return DELTA_OVERLAP;
  else
    return DELTA_DISJOINT;
}

std::string CreateNamePointer(uint16_t offset) {
  DCHECK_LE(offset, 0x3fff);
  offset |= kFlagNamePointer;
  char buf[2];
  base::WriteBigEndian(buf, offset);
  return std::string(buf, sizeof(buf));
}

uint16_t DnsQueryTypeToQtype(DnsQueryType dns_query_type) {
  switch (dns_query_type) {
    case DnsQueryType::UNSPECIFIED:
      NOTREACHED();
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
    case DnsQueryType::INTEGRITY:
      return dns_protocol::kExperimentalTypeIntegrity;
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
      NOTREACHED();
      return DnsQueryType::UNSPECIFIED;
  }
}

std::vector<DnsOverHttpsServerConfig> GetDohUpgradeServersFromDotHostname(
    const std::string& dot_server,
    const std::vector<std::string>& excluded_providers) {
  std::vector<DnsOverHttpsServerConfig> doh_servers;

  if (dot_server.empty())
    return doh_servers;

  for (const auto* entry : DohProviderEntry::GetList()) {
    if (base::Contains(excluded_providers, entry->provider))
      continue;

    if (base::Contains(entry->dns_over_tls_hostnames, dot_server)) {
      std::string server_method;
      CHECK(dns_util::IsValidDohTemplate(entry->dns_over_https_template,
                                         &server_method));
      doh_servers.emplace_back(entry->dns_over_https_template,
                               server_method == "POST");
    }
  }
  return doh_servers;
}

std::vector<DnsOverHttpsServerConfig> GetDohUpgradeServersFromNameservers(
    const std::vector<IPEndPoint>& dns_servers,
    const std::vector<std::string>& excluded_providers) {
  const auto entries =
      GetDohProviderEntriesFromNameservers(dns_servers, excluded_providers);
  std::vector<DnsOverHttpsServerConfig> doh_servers;
  doh_servers.reserve(entries.size());
  std::transform(entries.begin(), entries.end(),
                 std::back_inserter(doh_servers), [](const auto* entry) {
                   std::string server_method;
                   CHECK(dns_util::IsValidDohTemplate(
                       entry->dns_over_https_template, &server_method));
                   return DnsOverHttpsServerConfig(
                       entry->dns_over_https_template, server_method == "POST");
                 });
  return doh_servers;
}

std::string GetDohProviderIdForHistogramFromDohConfig(
    const DnsOverHttpsServerConfig& doh_server) {
  const auto& entries = DohProviderEntry::GetList();
  const auto it =
      std::find_if(entries.begin(), entries.end(), [&](const auto* entry) {
        return entry->dns_over_https_template == doh_server.server_template;
      });
  return it != entries.end() ? (*it)->provider : "Other";
}

std::string GetDohProviderIdForHistogramFromNameserver(
    const IPEndPoint& nameserver) {
  const auto entries = GetDohProviderEntriesFromNameservers({nameserver}, {});
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

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_UTIL_H_
#define NET_DNS_DNS_UTIL_H_

#include <cstdint>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "net/base/address_family.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_export.h"
#include "net/base/network_change_notifier.h"
#include "net/dns/public/dns_over_https_server_config.h"
#include "net/dns/public/dns_query_type.h"
#include "net/dns/public/secure_dns_mode.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class BigEndianReader;
}  // namespace base

namespace net {

// DNSDomainFromDot - convert a domain string to DNS format. From DJB's
// public domain DNS library. `dotted` may include only characters a-z, A-Z,
// 0-9, -, and _. Returns a result in DNS form: "\x03www\x06google\x03com\x00",
// or nullopt on invalid input.
//
//   dotted: a string in dotted form: "www.google.com"
NET_EXPORT absl::optional<std::vector<uint8_t>> DNSDomainFromDot(
    base::StringPiece dotted);

// DNSDomainFromUnrestrictedDot - convert a domain string to DNS format. Adapted
// from DJB's public domain DNS library. No validation of the characters in
// `dotted` is performed. Returns a result in DNS form:
// "\x03www\x06google\x03com\x00", or nullopt on invalid input.
//
//   dotted: a string in dotted form: "Foo Printer._tcp.local"
NET_EXPORT absl::optional<std::vector<uint8_t>> DNSDomainFromUnrestrictedDot(
    base::StringPiece dotted);

// Returns true iff `dotted` is acceptable to be encoded as a DNS name. That is
// that it is non-empty and fits size limitations. Also must match the expected
// structure of dot-separated labels, each non-empty and fitting within
// additional size limitations, and an optional dot at the end. See RFCs 1035
// and 2181.
//
// No validation is performed for correctness of characters within a label.
// As explained by RFC 2181, commonly cited rules for such characters are not
// DNS restrictions, but actually restrictions for Internet hostnames. For such
// validation, see IsCanonicalizedHostCompliant().
NET_EXPORT_PRIVATE bool IsValidDnsName(base::StringPiece dotted);

// Converts a domain in DNS format to a dotted string. Excludes the dot at the
// end.  Returns nullopt on malformed input.
//
// If `require_complete` is true, input will be considered malformed if it does
// not contain a terminating zero-length label. If false, assumes the standard
// terminating zero-length label at the end if not included in the input.
//
// DNS name compression (see RFC 1035, section 4.1.4) is disallowed and
// considered malformed. To handle a potentially compressed name, in a
// DnsResponse object, use DnsRecordParser::ReadName().
NET_EXPORT absl::optional<std::string> DnsDomainToString(
    base::span<const uint8_t> dns_name,
    bool require_complete = false);
NET_EXPORT absl::optional<std::string> DnsDomainToString(
    base::StringPiece dns_name,
    bool require_complete = false);
NET_EXPORT absl::optional<std::string> DnsDomainToString(
    base::BigEndianReader& reader,
    bool require_complete = false);

// Return the expanded template when no variables have corresponding values.
NET_EXPORT_PRIVATE std::string GetURLFromTemplateWithoutParameters(
    const std::string& server_template);

NET_EXPORT_PRIVATE
base::TimeDelta GetTimeDeltaForConnectionTypeFromFieldTrialOrDefault(
    const char* field_trial_name,
    base::TimeDelta default_delta,
    NetworkChangeNotifier::ConnectionType connection_type);

// Creates a 2-byte string that represents the name pointer defined in Section
// 4.1.1 of RFC 1035 for the given offset. The first two bits in the first byte
// of the name pointer are ones, and the rest 14 bits are given to `offset`,
// which specifies an offset from the start of the message for the pointed name.
// Note that `offset` must be less than 2^14 - 1 by definition.
NET_EXPORT std::string CreateNamePointer(uint16_t offset);

// Convert a DnsQueryType enum to the wire format integer representation.
NET_EXPORT_PRIVATE uint16_t DnsQueryTypeToQtype(DnsQueryType dns_query_type);

NET_EXPORT DnsQueryType
AddressFamilyToDnsQueryType(AddressFamily address_family);

// Uses the hardcoded upgrade mapping to discover DoH service(s) associated with
// a DoT hostname. Providers with a disabled `base::Feature` are not eligible
// for upgrade.
NET_EXPORT_PRIVATE std::vector<DnsOverHttpsServerConfig>
GetDohUpgradeServersFromDotHostname(const std::string& dot_server);

// Uses the hardcoded upgrade mapping to discover DoH service(s) associated with
// a list of insecure DNS servers. Server ordering is preserved across the
// mapping. Providers with a disabled `base::Feature` are not eligible for
// upgrade.
NET_EXPORT_PRIVATE std::vector<DnsOverHttpsServerConfig>
GetDohUpgradeServersFromNameservers(const std::vector<IPEndPoint>& dns_servers);

// Returns the provider id to use in UMA histogram names. If there is no
// provider id that matches `doh_server`, returns "Other".
NET_EXPORT_PRIVATE std::string GetDohProviderIdForHistogramFromServerConfig(
    const DnsOverHttpsServerConfig& doh_server);

// Returns the provider id to use in UMA histogram names. If there is no
// provider id that matches `nameserver`, returns "Other".
NET_EXPORT_PRIVATE std::string GetDohProviderIdForHistogramFromNameserver(
    const IPEndPoint& nameserver);

NET_EXPORT_PRIVATE std::string SecureDnsModeToString(
    const SecureDnsMode secure_dns_mode);

// std::map-compliant Compare for two dotted-format domain names. Returns true
// iff `lhs` is before `rhs` in strict weak ordering.
class NET_EXPORT_PRIVATE DomainNameComparator {
 public:
  bool operator()(base::StringPiece lhs, base::StringPiece rhs) const {
    return base::CompareCaseInsensitiveASCII(lhs, rhs) < 0;
  }
};

}  // namespace net

#endif  // NET_DNS_DNS_UTIL_H_

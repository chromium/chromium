// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_NAMES_UTIL_H_
#define NET_DNS_DNS_NAMES_UTIL_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "net/base/net_export.h"

namespace base {
class BigEndianReader;
}  // namespace base

// Various utilities for converting, validating, and comparing DNS names.
namespace net::dns_names_util {

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
NET_EXPORT_PRIVATE bool IsValidDnsName(base::StringPiece dotted_form_name);

// Like IsValidDnsName() but further validates `dotted_form_name` is not an IP
// address (with or without surrounding []) or localhost, as such names would
// not be suitable for DNS queries or for use as DNS record names or alias
// target names.
NET_EXPORT_PRIVATE bool IsValidDnsRecordName(
    base::StringPiece dotted_form_name);

// Convert a dotted-form DNS name to network wire format. Returns nullopt if
// input is not valid for conversion (equivalent validity can be checked using
// IsValidDnsName()). If `require_valid_internet_hostname` is true, also returns
// nullopt if input is not a valid internet hostname (equivalent validity can be
// checked using net::IsCanonicalizedHostCompliant()).
NET_EXPORT_PRIVATE std::optional<std::vector<uint8_t>> DottedNameToNetwork(
    base::StringPiece dotted_form_name,
    bool require_valid_internet_hostname = false);

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
NET_EXPORT_PRIVATE std::optional<std::string> NetworkToDottedName(
    base::span<const uint8_t> dns_network_wire_name,
    bool require_complete = false);
NET_EXPORT_PRIVATE std::optional<std::string> NetworkToDottedName(
    base::StringPiece dns_network_wire_name,
    bool require_complete = false);
NET_EXPORT_PRIVATE std::optional<std::string> NetworkToDottedName(
    base::BigEndianReader& reader,
    bool require_complete = false);

// Canonicalize `name` as a URL hostname if able. If unable (typically if a name
// is not a valid URL hostname), returns `name` without change because such a
// name could still be a valid DNS name.
NET_EXPORT_PRIVATE std::string UrlCanonicalizeNameIfAble(
    base::StringPiece name);

// std::map-compliant Compare for two domain names. Works for any valid
// dotted-format or network-wire-format names. Returns true iff `lhs` is before
// `rhs` in strict weak ordering.
class NET_EXPORT_PRIVATE DomainNameComparator {
 public:
  bool operator()(base::StringPiece lhs, base::StringPiece rhs) const {
    // This works for dotted format or network-wire format as long as the names
    // are valid because valid network-wire names have labels of max 63 bytes
    // and thus will never have label length prefixes high enough to be
    // misinterpreted as capital letters ('A' is 65).
    return base::CompareCaseInsensitiveASCII(lhs, rhs) < 0;
  }
};

}  // namespace net::dns_names_util

#endif  // NET_DNS_DNS_NAMES_UTIL_H_

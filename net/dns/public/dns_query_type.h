// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_PUBLIC_DNS_QUERY_TYPE_H_
#define NET_DNS_PUBLIC_DNS_QUERY_TYPE_H_

#include <string_view>

#include "base/containers/enum_set.h"
#include "base/containers/fixed_flat_map.h"
#include "net/base/net_export.h"

namespace net {

// DNS query type for HostResolver requests.
// See:
// https://www.iana.org/assignments/dns-parameters/dns-parameters.xhtml#dns-parameters-4
// CAUTION: When adding new entries, remember to update `MAX` and update
// kDnsQueryTypes below.
enum class DnsQueryType : uint8_t {
  UNSPECIFIED = 0,
  A = 1,
  AAAA = 2,
  TXT = 3,
  PTR = 4,
  SRV = 5,
  // 6 was INTEGRITY, used for an experiment (crbug.com/1052476).
  HTTPS = 7,
  // 8 was HTTPS_EXPERIMENTAL, used for an experiment (crbug.com/1052476).
  // When adding new entries, remember to update `MAX` and update kDnsQueryTypes
  // below.
  MAX = HTTPS
};

using DnsQueryTypeSet =
    base::EnumSet<DnsQueryType, DnsQueryType::UNSPECIFIED, DnsQueryType::MAX>;

inline constexpr auto kDnsQueryTypes =
    base::MakeFixedFlatMap<DnsQueryType, std::string_view>(
        {{DnsQueryType::UNSPECIFIED, "UNSPECIFIED"},
         {DnsQueryType::A, "A"},
         {DnsQueryType::AAAA, "AAAA"},
         {DnsQueryType::TXT, "TXT"},
         {DnsQueryType::PTR, "PTR"},
         {DnsQueryType::SRV, "SRV"},
         {DnsQueryType::HTTPS, "HTTPS"}});

// `true` iff `dns_query_type` is an address-resulting type, convertible to and
// from `net::AddressFamily`.
bool NET_EXPORT IsAddressType(DnsQueryType dns_query_type);

// `true` iff `dns_query_types` contains an address type. `dns_query_types` must
// be non-empty and must not contain `DnsQueryType::UNSPECIFIED`.
bool NET_EXPORT HasAddressType(DnsQueryTypeSet dns_query_types);

}  // namespace net

#endif  // NET_DNS_PUBLIC_DNS_QUERY_TYPE_H_

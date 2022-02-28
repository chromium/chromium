// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_PUBLIC_DNS_QUERY_TYPE_H_
#define NET_DNS_PUBLIC_DNS_QUERY_TYPE_H_

#include "base/containers/enum_set.h"
#include "base/containers/fixed_flat_map.h"
#include "base/strings/string_piece.h"
#include "net/base/net_export.h"

namespace net {

// DNS query type for HostResolver requests.
// See:
// https://www.iana.org/assignments/dns-parameters/dns-parameters.xhtml#dns-parameters-4
enum class DnsQueryType : uint8_t {
  UNSPECIFIED = 0,
  A,
  AAAA,
  TXT,
  PTR,
  SRV,
  INTEGRITY,
  HTTPS,
  HTTPS_EXPERIMENTAL,
  MAX = HTTPS_EXPERIMENTAL
};

using DnsQueryTypeSet =
    base::EnumSet<DnsQueryType, DnsQueryType::UNSPECIFIED, DnsQueryType::MAX>;

inline constexpr auto kDnsQueryTypes =
    base::MakeFixedFlatMap<DnsQueryType, base::StringPiece>(
        {{DnsQueryType::UNSPECIFIED, "UNSPECIFIED"},
         {DnsQueryType::A, "A"},
         {DnsQueryType::AAAA, "AAAA"},
         {DnsQueryType::TXT, "TXT"},
         {DnsQueryType::PTR, "PTR"},
         {DnsQueryType::SRV, "SRV"},
         {DnsQueryType::INTEGRITY, "INTEGRITY"},
         {DnsQueryType::HTTPS, "HTTPS"},
         {DnsQueryType::HTTPS_EXPERIMENTAL, "HTTPS_EXPERIMENTAL"}});

static_assert(std::size(kDnsQueryTypes) ==
                  static_cast<unsigned>(DnsQueryType::MAX) + 1,
              "All DnsQueryType values should be in kDnsQueryTypes.");

// `true` iff `dns_query_type` is an address-resulting type, convertible to and
// from `net::AddressFamily`.
bool NET_EXPORT IsAddressType(DnsQueryType dns_query_type);

// `true` iff `dns_query_types` contains an address type. `dns_query_types` must
// be non-empty and must not contain `DnsQueryType::UNSPECIFIED`.
bool NET_EXPORT HasAddressType(DnsQueryTypeSet dns_query_types);

}  // namespace net

#endif  // NET_DNS_PUBLIC_DNS_QUERY_TYPE_H_

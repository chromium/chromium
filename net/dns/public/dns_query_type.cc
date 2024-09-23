// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/public/dns_query_type.h"

#include "base/check.h"

namespace net {

bool IsAddressType(DnsQueryType dns_query_type) {
  // HostResolver treats UNSPECIFIED as A and/or AAAA depending on IPv4/IPv6
  // settings, so it is here considered an address type.
  return dns_query_type == DnsQueryType::UNSPECIFIED ||
         dns_query_type == DnsQueryType::A ||
         dns_query_type == DnsQueryType::AAAA;
}

bool HasAddressType(DnsQueryTypeSet dns_query_types) {
  DCHECK(!dns_query_types.empty());
  DCHECK(!dns_query_types.Has(DnsQueryType::UNSPECIFIED));
  return dns_query_types.Has(DnsQueryType::A) ||
         dns_query_types.Has(DnsQueryType::AAAA);
}

}  // namespace net

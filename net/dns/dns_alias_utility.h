// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_ALIAS_UTILITY_H_
#define NET_DNS_DNS_ALIAS_UTILITY_H_

#include <string>
#include <string_view>

#include "base/containers/flat_set.h"
#include "net/base/net_export.h"

namespace net::dns_alias_utility {

// Returns a reference to an empty set of DNS aliases.
NET_EXPORT_PRIVATE const base::flat_set<std::string>& EmptyDnsAliases();

// Returns a fixed up set of canonicalized aliases (i.e. aliases that are
// written as hostnames for canonical URLs). The set is stripped of "localhost",
// IP addresses, duplicates, the empty string, strings longer than
// `dns_protocol::kMaxCharNameLength` characters (with one extra character
// allowed for fully-qualified hostnames, i.e. hostnames ending with '.'), and
// any strings that fail to URL-canonicalize as hosts. The remaining aliases are
// replaced with their canonicalized forms.
NET_EXPORT_PRIVATE base::flat_set<std::string> FixUpDnsAliases(
    const base::flat_set<std::string>& aliases);
NET_EXPORT_PRIVATE base::flat_set<std::string> FixUpDnsAliases(
    const base::flat_set<std::string_view>& aliases);

}  // namespace net::dns_alias_utility

#endif  // NET_DNS_DNS_ALIAS_UTILITY_H_

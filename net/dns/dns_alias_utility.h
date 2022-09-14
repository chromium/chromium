// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_ALIAS_UTILITY_H_
#define NET_DNS_DNS_ALIAS_UTILITY_H_

#include <set>
#include <string>

#include "base/strings/string_piece.h"
#include "net/base/net_export.h"

namespace net::dns_alias_utility {

// Validates that `alias` represents a valid DNS alias name, e.g. CNAME, and
// then URL-canonicalizes the name. Returns empty string if not valid or unable
// to canonicalize.
NET_EXPORT_PRIVATE std::string ValidateAndCanonicalizeAlias(
    base::StringPiece alias);

// Returns a fixed up set of canonicalized aliases (i.e. aliases that are
// written as hostnames for canonical URLs). The set is stripped of "localhost",
// IP addresses, duplicates, the empty string, strings longer than
// `dns_protocol::kMaxCharNameLength` characters (with one extra character
// allowed for fully-qualified hostnames, i.e. hostnames ending with '.'), and
// any strings that fail to URL-canonicalize as hosts. The remaining aliases are
// replaced with their canonicalized forms.
NET_EXPORT_PRIVATE std::set<std::string> FixUpDnsAliases(
    const std::set<std::string>& aliases);

}  // namespace net::dns_alias_utility

#endif  // NET_DNS_DNS_ALIAS_UTILITY_H_

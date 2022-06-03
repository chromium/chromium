// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_ALIAS_UTILITY_H_
#define NET_DNS_DNS_ALIAS_UTILITY_H_

#include <string>
#include <vector>

#include "net/base/net_export.h"

namespace net {

namespace dns_alias_utility {

// Returns a sanitized list of canonicalized aliases (i.e. aliases that are
// written as hostnames for canonical URLs). The list is stripped of
// "localhost", IP addresses, duplicates, the empty string, strings longer
// than `dns_protocol::kMaxCharNameLength` characters (with one extra
// character allowed for fully-qualified hostnames, i.e. hostnames ending
// with '.'), and any strings that fail to URL-canonicalize as hosts. The
// remaining aliases are replaced with their canonicalized forms.
NET_EXPORT_PRIVATE std::vector<std::string> SanitizeDnsAliases(
    const std::vector<std::string>& aliases);

}  // namespace dns_alias_utility

}  // namespace net

#endif  // NET_DNS_DNS_ALIAS_UTILITY_H_

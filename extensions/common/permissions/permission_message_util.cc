// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/permissions/permission_message_util.h"

#include <stddef.h>
#include <vector>

#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/url_formatter/elide_url.h"
#include "extensions/common/url_pattern_set.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"
#include "url/url_constants.h"

using extensions::URLPatternSet;

namespace {

// Helper for GetDistinctHosts(): com > net > org > everything else.
bool RcdBetterThan(const std::string& a, const std::string& b) {
  if (a == b)
    return false;
  if (a == "com")
    return true;
  if (a == "net")
    return b != "com";
  if (a == "org")
    return b != "com" && b != "net";
  return false;
}

}  // namespace

namespace permission_message_util {

std::set<std::string> GetDistinctHosts(const URLPatternSet& host_patterns,
                                       bool include_rcd,
                                       bool exclude_file_scheme) {
  // Each item is a host split into two parts: host without RCDs and
  // current best RCD.
  using HostVector = base::StringPairs;
  HostVector hosts_best_rcd;
  for (const URLPattern& pattern : host_patterns) {
    if (exclude_file_scheme && pattern.scheme() == url::kFileScheme)
      continue;

    std::string host = pattern.host();
    if (!host.empty()) {
      // Convert the host into a secure format. For example, an IDN domain is
      // converted to punycode.
      host = base::UTF16ToUTF8(url_formatter::FormatUrlForSecurityDisplay(
          GURL(base::StringPrintf("%s%s%s", url::kHttpScheme,
                                  url::kStandardSchemeSeparator, host.c_str())),
          url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS));
    }

    // Add the subdomain wildcard back to the host, if necessary.
    if (pattern.match_subdomains())
      host = "*." + host;

    // If the host has an RCD, split it off so we can detect duplicates.

    std::string rcd;
    size_t reg_len =
        net::registry_controlled_domains::PermissiveGetHostRegistryLength(
            host, net::registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES,
            net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
    if (reg_len && reg_len != std::string::npos) {
      if (include_rcd)  // else leave rcd empty
        rcd = host.substr(host.size() - reg_len);
      host = host.substr(0, host.size() - reg_len);
    }

    // Check if we've already seen this host.
    auto it = hosts_best_rcd.begin();
    for (; it != hosts_best_rcd.end(); ++it) {
      if (it->first == host)
        break;
    }
    // If this host was found, replace the RCD if this one is better.
    if (it != hosts_best_rcd.end()) {
      if (include_rcd && RcdBetterThan(rcd, it->second))
        it->second = rcd;
    } else {  // Previously unseen host, append it.
      hosts_best_rcd.push_back(std::make_pair(host, rcd));
    }
  }

  // Build up the result by concatenating hosts and RCDs.
  std::set<std::string> distinct_hosts;
  for (const auto& host_rcd : hosts_best_rcd)
    distinct_hosts.insert(host_rcd.first + host_rcd.second);
  return distinct_hosts;
}

}  // namespace permission_message_util

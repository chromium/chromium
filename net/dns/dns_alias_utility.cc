// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_alias_utility.h"

#include <string>
#include <unordered_set>
#include <vector>

#include "base/check.h"
#include "net/base/url_util.h"
#include "net/dns/public/dns_protocol.h"
#include "url/url_canon.h"
#include "url/url_canon_stdstring.h"

namespace net {

namespace {

bool IsValidAliasWithCanonicalOutput(const std::string& alias,
                                     std::string* out_canonicalized_alias) {
  DCHECK(out_canonicalized_alias);

  // Disallow empty hostnames, hostnames longer than
  // `dns_protocol::kMaxCharNameLength` characters (with one extra character
  // allowed for fully-qualified hostnames, i.e. hostnames ending with '.'),
  // and "localhost".
  if (alias.empty() || alias.size() > dns_protocol::kMaxCharNameLength + 1 ||
      (alias.size() == dns_protocol::kMaxCharNameLength + 1 &&
       alias.back() != '.') ||
      HostStringIsLocalhost(alias)) {
    return false;
  }

  url::StdStringCanonOutput output(out_canonicalized_alias);
  url::CanonHostInfo host_info;
  const char* alias_spec = alias.c_str();
  url::Component host(0, alias.size());

  url::CanonicalizeHostVerbose(alias_spec, host, &output, &host_info);
  output.Complete();

  return host_info.family == url::CanonHostInfo::Family::NEUTRAL;
}

}  // namespace

namespace dns_alias_utility {

std::set<std::string> FixUpDnsAliases(const std::set<std::string>& aliases) {
  std::set<std::string> fixed_aliases;

  for (const auto& alias : aliases) {
    std::string canonicalized_alias;

    if (IsValidAliasWithCanonicalOutput(alias, &canonicalized_alias)) {
      fixed_aliases.insert(std::move(canonicalized_alias));
    }
  }

  return fixed_aliases;
}

}  // namespace dns_alias_utility

}  // namespace net

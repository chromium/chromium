// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_alias_utility.h"

#include <set>
#include <string>

#include "base/strings/string_piece.h"
#include "net/base/url_util.h"
#include "net/dns/public/dns_protocol.h"
#include "url/third_party/mozilla/url_parse.h"
#include "url/url_canon.h"
#include "url/url_canon_stdstring.h"

namespace net::dns_alias_utility {

std::string ValidateAndCanonicalizeAlias(base::StringPiece alias) {
  // Disallow empty hostnames, hostnames longer than
  // `dns_protocol::kMaxCharNameLength` characters (with one extra character
  // allowed for fully-qualified hostnames, i.e. hostnames ending with '.'),
  // and "localhost".
  if (alias.empty() || alias.size() > dns_protocol::kMaxCharNameLength + 1 ||
      (alias.size() == dns_protocol::kMaxCharNameLength + 1 &&
       alias.back() != '.') ||
      HostStringIsLocalhost(alias)) {
    return "";
  }

  std::string canonicalized_alias;
  url::StdStringCanonOutput output(&canonicalized_alias);
  url::CanonHostInfo host_info;
  const char* alias_spec = alias.data();
  url::Component host(0, alias.size());

  url::CanonicalizeHostVerbose(alias_spec, host, &output, &host_info);

  if (host_info.family != url::CanonHostInfo::Family::NEUTRAL)
    return "";

  output.Complete();
  return canonicalized_alias;
}

std::set<std::string> FixUpDnsAliases(const std::set<std::string>& aliases) {
  std::set<std::string> fixed_aliases;

  for (const std::string& alias : aliases) {
    std::string canonicalized_alias = ValidateAndCanonicalizeAlias(alias);
    if (!canonicalized_alias.empty())
      fixed_aliases.insert(std::move(canonicalized_alias));
  }

  return fixed_aliases;
}

}  // namespace net::dns_alias_utility

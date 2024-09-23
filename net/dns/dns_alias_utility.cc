// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_alias_utility.h"

#include <set>
#include <string>

#include "net/base/url_util.h"
#include "net/dns/dns_names_util.h"
#include "net/dns/public/dns_protocol.h"
#include "url/third_party/mozilla/url_parse.h"
#include "url/url_canon.h"
#include "url/url_canon_stdstring.h"

namespace net::dns_alias_utility {

std::set<std::string> FixUpDnsAliases(const std::set<std::string>& aliases) {
  std::set<std::string> fixed_aliases;

  for (const std::string& alias : aliases) {
    if (!dns_names_util::IsValidDnsRecordName(alias)) {
      continue;
    }

    std::string canonicalized_alias;
    url::StdStringCanonOutput output(&canonicalized_alias);
    url::CanonHostInfo host_info;
    url::CanonicalizeHostVerbose(alias.data(), url::Component(0, alias.size()),
                                 &output, &host_info);

    if (host_info.family == url::CanonHostInfo::Family::BROKEN) {
      continue;
    }

    // IP addresses should have been rejected by IsValidDnsRecordName().
    DCHECK_NE(host_info.family, url::CanonHostInfo::Family::IPV4);
    DCHECK_NE(host_info.family, url::CanonHostInfo::Family::IPV6);

    output.Complete();
    fixed_aliases.insert(std::move(canonicalized_alias));
  }

  return fixed_aliases;
}

}  // namespace net::dns_alias_utility

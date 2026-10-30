// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_alias_utility.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/no_destructor.h"
#include "net/base/url_util.h"
#include "net/dns/dns_names_util.h"
#include "net/dns/public/dns_protocol.h"
#include "url/third_party/mozilla/url_parse.h"
#include "url/url_canon.h"
#include "url/url_canon_stdstring.h"

namespace net::dns_alias_utility {

namespace {

template <typename StringType>
base::flat_set<std::string> FixUpDnsAliasesImpl(
    const base::flat_set<StringType>& aliases) {
  std::vector<std::string> fixed_aliases;
  fixed_aliases.reserve(aliases.size());

  for (std::string_view alias : aliases) {
    if (!dns_names_util::IsValidDnsRecordName(alias)) {
      continue;
    }

    std::string canonicalized_alias;
    url::StdStringCanonOutput output(&canonicalized_alias);
    url::CanonHostInfo host_info;
    url::CanonicalizeHostVerbose(alias, url::Component(0, alias.size()),
                                 &output, &host_info);

    if (host_info.family == url::CanonHostInfo::Family::BROKEN) {
      continue;
    }

    // IP addresses should have been rejected by IsValidDnsRecordName().
    DCHECK_NE(host_info.family, url::CanonHostInfo::Family::IPV4);
    DCHECK_NE(host_info.family, url::CanonHostInfo::Family::IPV6);

    output.Complete();
    fixed_aliases.push_back(std::move(canonicalized_alias));
  }

  return base::flat_set<std::string>(std::move(fixed_aliases));
}

}  // namespace

const base::flat_set<std::string>& EmptyDnsAliases() {
  static const base::NoDestructor<base::flat_set<std::string>> kEmptyAliases;
  return *kEmptyAliases;
}

base::flat_set<std::string> FixUpDnsAliases(
    const base::flat_set<std::string>& aliases) {
  return FixUpDnsAliasesImpl(aliases);
}

base::flat_set<std::string> FixUpDnsAliases(
    const base::flat_set<std::string_view>& aliases) {
  return FixUpDnsAliasesImpl(aliases);
}

}  // namespace net::dns_alias_utility

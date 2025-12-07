// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/does_url_match_filter.h"

#include <string_view>

#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace net {

namespace {

bool MatchesOriginOrDomain(const base::flat_set<url::Origin>& origins,
                           const base::flat_set<std::string>& domains,
                           const url::Origin& origin) {
  if (origins.contains(origin)) {
    return true;
  }

  // Avoid the expensive GetDomainAndRegistry() call when possible.
  if (domains.empty()) {
    return false;
  }

  const std::string_view url_registerable_domain =
      GetDomainAndRegistryAsStringPiece(
          origin, registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);

  const std::string_view domain =
      url_registerable_domain.empty() ? origin.host() : url_registerable_domain;

  return domains.contains(domain);
}

}  // namespace

bool DoesUrlMatchFilter(UrlFilterType filter_type,
                        const base::flat_set<url::Origin>& origins,
                        const base::flat_set<std::string>& domains,
                        const GURL& url) {
  auto origin = url::Origin::Create(url);
  return MatchesOriginOrDomain(origins, domains, origin) ==
         (filter_type == UrlFilterType::kTrueIfMatches);
}

}  // namespace net

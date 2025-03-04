// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/does_url_match_filter.h"

#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace net {

bool DoesUrlMatchFilter(UrlFilterType filter_type,
                        const std::set<url::Origin>& origins,
                        const std::set<std::string>& domains,
                        const GURL& url) {
  std::string url_registerable_domain =
      registry_controlled_domains::GetDomainAndRegistry(
          url, registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  bool found_domain = (domains.find(url_registerable_domain != ""
                                        ? url_registerable_domain
                                        : url.host()) != domains.end());

  bool found_origin = (origins.find(url::Origin::Create(url)) != origins.end());

  return ((found_domain || found_origin) ==
          (filter_type == UrlFilterType::kTrueIfMatches));
}

}  // namespace net

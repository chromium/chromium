// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CORS_ORIGIN_ACCESS_ENTRY_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CORS_ORIGIN_ACCESS_ENTRY_H_

#include <string>

#include "base/component_export.h"
#include "base/macros.h"
#include "services/network/public/mojom/cors.mojom-shared.h"

namespace url {
class Origin;
}  // namespace url

namespace network {

namespace cors {

// A class to hold a protocol and host pair and to provide methods to determine
// if a given origin or domain matches to the pair. The class can have a setting
// to control if the matching methods accept a partial match.
class COMPONENT_EXPORT(NETWORK_CPP) OriginAccessEntry final {
 public:
  // A enum to represent a mode if matching functions can accept a partial match
  // for sub-domains, or for registerable domains.
  enum MatchMode {
    // 'www.example.com' matches an OriginAccessEntry for 'example.com'
    kAllowSubdomains,

    // 'www.example.com' matches an OriginAccessEntry for 'not-www.example.com'
    kAllowRegisterableDomains,

    // 'www.example.com' does not match an OriginAccessEntry for 'example.com'
    kDisallowSubdomains,
  };

  enum MatchResult {
    kMatchesOrigin,
    kMatchesOriginButIsPublicSuffix,
    kDoesNotMatchOrigin
  };

  // If host is empty string and MatchMode is not DisallowSubdomains, the entry
  // will match all domains in the specified protocol.
  // IPv6 addresses must include brackets (e.g.
  // '[2001:db8:85a3::8a2e:370:7334]', not '2001:db8:85a3::8a2e:370:7334').
  // The priority argument is used to break ties when multiple entries
  // match.
  OriginAccessEntry(
      const std::string& protocol,
      const std::string& host,
      MatchMode match_mode,
      const network::mojom::CORSOriginAccessMatchPriority priority =
          network::mojom::CORSOriginAccessMatchPriority::kDefaultPriority);
  OriginAccessEntry(OriginAccessEntry&& from);

  // 'matchesOrigin' requires a protocol match (e.g. 'http' != 'https').
  // 'matchesDomain' relaxes this constraint.
  MatchResult MatchesOrigin(const url::Origin& origin) const;
  MatchResult MatchesDomain(const url::Origin& domain) const;

  bool host_is_ip_address() const { return host_is_ip_address_; }
  network::mojom::CORSOriginAccessMatchPriority priority() const {
    return priority_;
  }
  const std::string& registerable_domain() const {
    return registerable_domain_;
  }

 private:
  const std::string protocol_;
  const std::string host_;
  const MatchMode match_mode_;
  network::mojom::CORSOriginAccessMatchPriority priority_;
  const bool host_is_ip_address_;

  std::string registerable_domain_;
  bool host_is_public_suffix_;

  DISALLOW_COPY_AND_ASSIGN(OriginAccessEntry);
};

}  // namespace cors

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CORS_ORIGIN_ACCESS_ENTRY_H_

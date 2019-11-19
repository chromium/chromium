// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CORS_ORIGIN_ACCESS_ENTRY_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CORS_ORIGIN_ACCESS_ENTRY_H_

#include <string>

#include "base/component_export.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "services/network/public/mojom/cors.mojom-shared.h"
#include "services/network/public/mojom/cors_origin_pattern.mojom-shared.h"

namespace url {
class Origin;
}  // namespace url

namespace network {

namespace mojom {
class CorsOriginPattern;
}  // namespace mojom

namespace cors {

// A class to hold a protocol and domain pair and to provide methods to
// determine if a given origin or domain matches to the pair. The class can have
// a setting to control if the matching methods accept a partial match.
class COMPONENT_EXPORT(NETWORK_CPP) OriginAccessEntry final {
 public:
  enum MatchResult {
    kMatchesOrigin,
    kMatchesOriginButIsPublicSuffix,
    kDoesNotMatchOrigin
  };

  // If domain is empty string and CorsDomainMatchMode is not
  // DisallowSubdomains, the entry will match all domains in the specified
  // protocol.
  // IPv6 addresses must include brackets (e.g.
  // '[2001:db8:85a3::8a2e:370:7334]', not '2001:db8:85a3::8a2e:370:7334').
  // The priority argument is used to break ties when multiple entries match.
  // If CorsPortMatchMode::kAllowOnlySpecifiedPort is used, |port| is used in
  // MatchesOrigin().
  OriginAccessEntry(const std::string& protocol,
                    const std::string& domain,
                    const uint16_t port,
                    const mojom::CorsDomainMatchMode domain_match_mode,
                    const mojom::CorsPortMatchMode port_match_mode,
                    const mojom::CorsOriginAccessMatchPriority priority =
                        mojom::CorsOriginAccessMatchPriority::kDefaultPriority);
  OriginAccessEntry(OriginAccessEntry&& from);

  // MatchesOrigin requires a protocol match (e.g. 'http' != 'https'), and a
  // port match if kAllowOnlySpecifiedPort is specified. MatchesDomain relaxes
  // these constraints.
  MatchResult MatchesOrigin(const url::Origin& origin) const;
  MatchResult MatchesDomain(const std::string& domain) const;

  bool host_is_ip_address() const { return host_is_ip_address_; }
  mojom::CorsOriginAccessMatchPriority priority() const { return priority_; }
  const std::string& registrable_domain() const { return registrable_domain_; }

  // Creates mojom::CorsOriginPattern instance that represents |this|
  // OriginAccessEntry instance.
  mojo::StructPtr<mojom::CorsOriginPattern> CreateCorsOriginPattern() const;

 private:
  const std::string protocol_;
  const std::string host_;
  const uint16_t port_;
  const mojom::CorsDomainMatchMode domain_match_mode_;
  const mojom::CorsPortMatchMode port_match_mode_;
  const mojom::CorsOriginAccessMatchPriority priority_;
  const bool host_is_ip_address_;

  std::string registrable_domain_;
  bool host_is_public_suffix_;

  DISALLOW_COPY_AND_ASSIGN(OriginAccessEntry);
};

}  // namespace cors

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CORS_ORIGIN_ACCESS_ENTRY_H_

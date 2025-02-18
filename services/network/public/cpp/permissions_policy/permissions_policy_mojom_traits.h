// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_PERMISSIONS_POLICY_PERMISSIONS_POLICY_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_PERMISSIONS_POLICY_PERMISSIONS_POLICY_MOJOM_TRAITS_H_

#include <string>

#include "base/component_export.h"
#include "services/network/public/cpp/permissions_policy/origin_with_possible_wildcards.h"
#include "services/network/public/cpp/permissions_policy/permissions_policy_declaration.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy.mojom-shared.h"

namespace mojo {

template <>
class COMPONENT_EXPORT(NETWORK_CPP)
    StructTraits<network::mojom::OriginWithPossibleWildcardsDataView,
                 network::OriginWithPossibleWildcards> {
 public:
  static const std::string& scheme(const network::OriginWithPossibleWildcards&
                                       origin_with_possible_wildcards) {
    return origin_with_possible_wildcards.csp_source.scheme;
  }
  static const std::string& host(const network::OriginWithPossibleWildcards&
                                     origin_with_possible_wildcards) {
    return origin_with_possible_wildcards.csp_source.host;
  }
  static int port(const network::OriginWithPossibleWildcards&
                      origin_with_possible_wildcards) {
    return origin_with_possible_wildcards.csp_source.port;
  }
  static bool is_host_wildcard(const network::OriginWithPossibleWildcards&
                                   origin_with_possible_wildcards) {
    return origin_with_possible_wildcards.csp_source.is_host_wildcard;
  }
  static bool is_port_wildcard(const network::OriginWithPossibleWildcards&
                                   origin_with_possible_wildcards) {
    return origin_with_possible_wildcards.csp_source.is_port_wildcard;
  }

  static bool Read(network::mojom::OriginWithPossibleWildcardsDataView in,
                   network::OriginWithPossibleWildcards* out);
};

template <>
class COMPONENT_EXPORT(NETWORK_CPP)
    StructTraits<network::mojom::ParsedPermissionsPolicyDeclarationDataView,
                 network::ParsedPermissionsPolicyDeclaration> {
 public:
  static network::mojom::PermissionsPolicyFeature feature(
      const network::ParsedPermissionsPolicyDeclaration& policy) {
    return policy.feature;
  }
  static const std::vector<network::OriginWithPossibleWildcards>&
  allowed_origins(const network::ParsedPermissionsPolicyDeclaration& policy) {
    return policy.allowed_origins;
  }
  static const std::optional<url::Origin>& self_if_matches(
      const network::ParsedPermissionsPolicyDeclaration& policy) {
    return policy.self_if_matches;
  }
  static bool matches_all_origins(
      const network::ParsedPermissionsPolicyDeclaration& policy) {
    return policy.matches_all_origins;
  }
  static bool matches_opaque_src(
      const network::ParsedPermissionsPolicyDeclaration& policy) {
    return policy.matches_opaque_src;
  }
  static const std::optional<std::string>& reporting_endpoint(
      const network::ParsedPermissionsPolicyDeclaration& policy) {
    return policy.reporting_endpoint;
  }

  static bool Read(
      network::mojom::ParsedPermissionsPolicyDeclarationDataView in,
      network::ParsedPermissionsPolicyDeclaration* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_PERMISSIONS_POLICY_PERMISSIONS_POLICY_MOJOM_TRAITS_H_

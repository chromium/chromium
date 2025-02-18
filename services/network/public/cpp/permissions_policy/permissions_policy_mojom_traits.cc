// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/permissions_policy/permissions_policy_mojom_traits.h"

#include "services/network/public/cpp/permissions_policy/origin_with_possible_wildcards.h"
#include "services/network/public/cpp/permissions_policy/permissions_policy_declaration.h"
#include "url/mojom/origin_mojom_traits.h"

namespace mojo {

bool StructTraits<network::mojom::OriginWithPossibleWildcardsDataView,
                  network::OriginWithPossibleWildcards>::
    Read(network::mojom::OriginWithPossibleWildcardsDataView in,
         network::OriginWithPossibleWildcards* out) {
  out->csp_source.is_host_wildcard = in.is_host_wildcard();
  out->csp_source.is_port_wildcard = in.is_port_wildcard();
  out->csp_source.port = in.port();
  if (!in.ReadScheme(&out->csp_source.scheme) ||
      !in.ReadHost(&out->csp_source.host)) {
    return false;
  }
  // For local files the host might be empty, but the scheme cannot be.
  return out->csp_source.scheme.length() != 0;
}

bool StructTraits<network::mojom::ParsedPermissionsPolicyDeclarationDataView,
                  network::ParsedPermissionsPolicyDeclaration>::
    Read(network::mojom::ParsedPermissionsPolicyDeclarationDataView in,
         network::ParsedPermissionsPolicyDeclaration* out) {
  out->matches_all_origins = in.matches_all_origins();
  out->matches_opaque_src = in.matches_opaque_src();
  return in.ReadFeature(&out->feature) &&
         in.ReadAllowedOrigins(&out->allowed_origins) &&
         in.ReadSelfIfMatches(&out->self_if_matches);
}

}  // namespace mojo

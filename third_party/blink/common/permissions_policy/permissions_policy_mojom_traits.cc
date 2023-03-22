// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/common/permissions_policy/permissions_policy_mojom_traits.h"

#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "url/mojom/origin_mojom_traits.h"
#include "url/origin.h"

namespace mojo {

bool StructTraits<network::mojom::CSPSourceDataView,
                  blink::OriginWithPossibleWildcards>::
    Read(network::mojom::CSPSourceDataView in,
         blink::OriginWithPossibleWildcards* out) {
  // We do not support any wildcard types besides host
  // based ones for now.
  out->has_subdomain_wildcard = in.is_host_wildcard();
  std::string scheme;
  std::string host;
  if (!in.ReadScheme(&scheme) || !in.ReadHost(&host)) {
    return false;
  }
  absl::optional<url::Origin> maybe_origin =
      url::Origin::UnsafelyCreateTupleOriginWithoutNormalization(scheme, host,
                                                                 in.port());
  if (!maybe_origin) {
    return false;
  }
  out->origin = *maybe_origin;

  // Origins cannot be opaque.
  return !out->origin.opaque();
}

bool StructTraits<blink::mojom::ParsedPermissionsPolicyDeclarationDataView,
                  blink::ParsedPermissionsPolicyDeclaration>::
    Read(blink::mojom::ParsedPermissionsPolicyDeclarationDataView in,
         blink::ParsedPermissionsPolicyDeclaration* out) {
  out->matches_all_origins = in.matches_all_origins();
  out->matches_opaque_src = in.matches_opaque_src();
  return in.ReadFeature(&out->feature) &&
         in.ReadAllowedOrigins(&out->allowed_origins) &&
         in.ReadSelfIfMatches(&out->self_if_matches);
}

}  // namespace mojo

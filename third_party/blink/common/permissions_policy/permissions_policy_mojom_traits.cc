// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/common/permissions_policy/permissions_policy_mojom_traits.h"

#include "services/network/public/cpp/permissions_policy/permissions_policy_mojom_traits.h"
#include "url/mojom/origin_mojom_traits.h"
#include "url/origin.h"

namespace mojo {

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

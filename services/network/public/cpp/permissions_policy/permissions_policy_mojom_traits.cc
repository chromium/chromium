// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/permissions_policy/permissions_policy_mojom_traits.h"

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

}  // namespace mojo

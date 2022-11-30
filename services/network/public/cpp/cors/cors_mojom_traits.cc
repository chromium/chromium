// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cors/cors_mojom_traits.h"

#include "mojo/public/cpp/base/unguessable_token_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<network::mojom::CorsErrorStatusDataView,
                  network::CorsErrorStatus>::
    Read(network::mojom::CorsErrorStatusDataView data,
         network::CorsErrorStatus* out) {
  if (!data.ReadFailedParameter(&out->failed_parameter) ||
      !data.ReadIssueId(&out->issue_id)) {
    return false;
  }

  out->cors_error = data.cors_error();
  out->target_address_space = data.target_address_space();
  out->resource_address_space = data.resource_address_space();
  out->has_authorization_covered_by_wildcard_on_preflight =
      data.has_authorization_covered_by_wildcard_on_preflight();
  return true;
}

}  // namespace mojo

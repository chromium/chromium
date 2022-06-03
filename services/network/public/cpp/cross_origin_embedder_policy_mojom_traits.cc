// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cross_origin_embedder_policy_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<network::mojom::CrossOriginEmbedderPolicyDataView,
                  network::CrossOriginEmbedderPolicy>::
    Read(network::mojom::CrossOriginEmbedderPolicyDataView input,
         network::CrossOriginEmbedderPolicy* output) {
  network::CrossOriginEmbedderPolicy result;

  if (input.ReadValue(&result.value) &&
      input.ReadReportingEndpoint(&result.reporting_endpoint) &&
      input.ReadReportOnlyValue(&result.report_only_value) &&
      input.ReadReportOnlyReportingEndpoint(
          &result.report_only_reporting_endpoint)) {
    *output = std::move(result);
    return true;
  }
  return false;
}

}  // namespace mojo

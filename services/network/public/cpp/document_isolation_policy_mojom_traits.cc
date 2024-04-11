// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/document_isolation_policy_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<network::mojom::DocumentIsolationPolicyDataView,
                  network::DocumentIsolationPolicy>::
    Read(network::mojom::DocumentIsolationPolicyDataView input,
         network::DocumentIsolationPolicy* output) {
  network::DocumentIsolationPolicy result;

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

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cross_origin_opener_policy_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<network::mojom::CrossOriginOpenerPolicyDataView,
                  network::CrossOriginOpenerPolicy>::
    Read(network::mojom::CrossOriginOpenerPolicyDataView input,
         network::CrossOriginOpenerPolicy* output) {
  network::CrossOriginOpenerPolicy result;

  if (input.ReadValue(&result.value) &&
      input.ReadReportingEndpoint(&result.reporting_endpoint) &&
      input.ReadReportOnlyValue(&result.report_only_value) &&
      input.ReadReportOnlyReportingEndpoint(
          &result.report_only_reporting_endpoint) &&
      input.ReadSoapByDefaultValue(&result.soap_by_default_value)) {
    *output = std::move(result);
    return true;
  }
  return false;
}

}  // namespace mojo

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/reporting_api_endpoint_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<network::mojom::ReportingApiEndpointDataView,
                  net::ReportingEndpoint>::
    Read(network::mojom::ReportingApiEndpointDataView data,
         net::ReportingEndpoint* out) {
  if (!data.ReadUrl(&out->info.url)) {
    return false;
  }
  if (!data.ReadOrigin(&out->group_key.origin)) {
    return false;
  }
  if (!data.ReadGroupName(&out->group_key.group_name)) {
    return false;
  }
  if (!data.ReadNetworkAnonymizationKey(
          &out->group_key.network_anonymization_key)) {
    return false;
  }
  if (!data.ReadReportingSource(&out->group_key.reporting_source)) {
    return false;
  }

  out->stats.attempted_uploads = data.attempted_uploads();
  out->stats.successful_uploads = data.successful_uploads();
  out->stats.attempted_reports = data.attempted_reports();
  out->stats.successful_reports = data.successful_reports();
  out->info.priority = data.priority();
  out->info.weight = data.weight();
  return true;
}

}  // namespace mojo
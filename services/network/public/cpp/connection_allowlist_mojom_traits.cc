// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/connection_allowlist_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<network::mojom::ConnectionAllowlistDataView,
                  network::ConnectionAllowlist>::
    Read(network::mojom::ConnectionAllowlistDataView data,
         network::ConnectionAllowlist* out) {
  if (!data.ReadAllowlist(&out->allowlist) ||
      !data.ReadReportingEndpoint(&out->reporting_endpoint) ||
      !data.ReadIssues(&out->issues)) {
    return false;
  }
  return true;
}

// static
bool StructTraits<network::mojom::ConnectionAllowlistsDataView,
                  network::ConnectionAllowlists>::
    Read(network::mojom::ConnectionAllowlistsDataView data,
         network::ConnectionAllowlists* out) {
  if (!data.ReadEnforced(&out->enforced) ||
      !data.ReadReportOnly(&out->report_only)) {
    return false;
  }
  return true;
}

}  // namespace mojo

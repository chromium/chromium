// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/attribution_mojom_traits.h"

#include "services/network/public/cpp/attribution_reporting_runtime_features.h"
#include "services/network/public/mojom/attribution.mojom-shared.h"

namespace mojo {

// static
bool StructTraits<network::mojom::AttributionReportingRuntimeFeaturesDataView,
                  network::AttributionReportingRuntimeFeatures>::
    cross_app_web_enabled(
        network::AttributionReportingRuntimeFeatures runtime_features) {
  return runtime_features.Has(
      network::AttributionReportingRuntimeFeature::kCrossAppWeb);
}

// static
bool StructTraits<network::mojom::AttributionReportingRuntimeFeaturesDataView,
                  network::AttributionReportingRuntimeFeatures>::
    Read(network::mojom::AttributionReportingRuntimeFeaturesDataView data,
         network::AttributionReportingRuntimeFeatures* out) {
  if (data.cross_app_web_enabled()) {
    out->Put(network::AttributionReportingRuntimeFeature::kCrossAppWeb);
  }
  return true;
}

}  // namespace mojo

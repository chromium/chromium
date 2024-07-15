// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_ATTRIBUTION_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_ATTRIBUTION_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "services/network/public/cpp/attribution_reporting_runtime_features.h"
#include "services/network/public/mojom/attribution.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_ATTRIBUTION_MOJOM_TRAITS)
    StructTraits<network::mojom::AttributionReportingRuntimeFeaturesDataView,
                 network::AttributionReportingRuntimeFeatures> {
  static bool cross_app_web_enabled(
      network::AttributionReportingRuntimeFeatures runtime_features);

  static bool Read(
      network::mojom::AttributionReportingRuntimeFeaturesDataView data,
      network::AttributionReportingRuntimeFeatures* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_ATTRIBUTION_MOJOM_TRAITS_H_
